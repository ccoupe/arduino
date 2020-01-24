/*********
 * Use PIR motion sensor - Keystudio
 * After a trigger actve, device has a pause that keeps it (or anything?) from running 
 * for a second or 2 or 3 (including timer ISR). It's OK for my purposes.
 * 
 * Accepts command delay= nn or conf={"active_hold": nn} 
 * Json sent by newer drivers
 *
*********/
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Arduino_JSON.h>

// Replace the next variables with your SSID/Password combination
const char* ssid = "CJCNET";
const char* password = "LostAgain2";

// Add your MQTT Broker IP address, device name, topic 
const char* mqtt_server = "192.168.1.7";
#define MQTT_DEVICE "ESP32_Keystudio1"
#define MQTT_TOPIC "sensors/family/keystudio1"
#define MQTT_CMD   "sensors/family/keystudio1_control"
//#define MQTT_CMD   "sensors/family/keystudio1"

boolean turnedOn = true;  // controls whether device sends to MQTT
#define ACTIVE 1
#define INACTIVE 0
int state = INACTIVE;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;

// LED Pin - built-in blue led on some boards
//const int led = 2;

// Keystudio pir motion sensor
#define  pirSensor 23
volatile boolean havePir = false;

#define DelayToOff 45
unsigned int delaySeconds = DelayToOff;   // we may want to adjust this variable dynamically
int motionCount = 0;

// Interrupt handler for motion on AM312 pir
void IRAM_ATTR intrPir() {
  //Serial.println("PIR ISR");
  havePir = true;
}

void IRAM_ATTR onTimer(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  isrCounter++;
  lastIsrAt = millis();
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
}

void send_config() {
  char *tmpl = "conf={\"active_hold\":%d}";
  char msg[50];
  sprintf(msg, tmpl, delaySeconds);
  Serial.print("sending ");
  Serial.println(msg);
  mqtt_publish(MQTT_TOPIC, "active");
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin((char *)ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  // convert byte* to String
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    messageTemp += (char)payload[i];
  }
  Serial.println();

  // If a message is received on the topic 
  // check if the message is either "enabled", "disabled" or
  // 'active', 'inactive' since we get the what we sent. Problem?
  // Set 'turnedOn' appropriately
  if (String(topic).equals(MQTT_CMD)) {
    if(messageTemp == "enable") {
      turnedOn = true;
      state = INACTIVE;
      Serial.println("mqtt sent an enable");
    } else if (messageTemp == "disable") {
      turnedOn = false;
      state = INACTIVE;
      Serial.println("mqtt sent a disable");
    } else if (messageTemp.startsWith("conf=")) {
      JSONVar myObject = JSON.parse(messageTemp.substring(5));
      Serial.println(myObject);
      if (myObject.hasOwnProperty("active_hold")) {
        int d =  myObject["active_hold"];
        if (d < 5)
          d = 5;
        else if (d > 3600)
          d = 3600;
        delaySeconds = d;
      }
    } else if (messageTemp.startsWith("conf")) {
      JSONVar myObject;
      myObject["active_hold"] = (int)delaySeconds;
      String jsonString = JSON.stringify(myObject);
      char str[50];
      sprintf(str,"conf=%s",jsonString.c_str());
      mqtt_publish(MQTT_TOPIC, str);
    } else if (messageTemp.startsWith("delay=")) {
      int v = messageTemp.substring(6).toInt();
      if (v >= 5) {
        Serial.print("mqtt delay = "); Serial.println(v);
        delaySeconds = v;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting");
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();

  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timer = timerBegin(0, 80, true);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &onTimer, true);

  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer, 1000000, true);

  // Start an alarm
  timerAlarmEnable(timer);

  //motionSemaphore = xSemaphoreCreateBinary();
  // PIR Motion Sensor mode INPUT_PULLUP
  pinMode(pirSensor, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pirSensor), intrPir, RISING);
  
  //pinMode(led, OUTPUT);
  //digitalWrite(led, LOW);
}


void reconnect() {
  // Loop until we're reconnected
  int cnt = 1;
  int len = 5;
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_DEVICE)) {
      Serial.println("connected");
      // Subscribe
      client.subscribe(MQTT_CMD);
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      Serial.print(" try again in ");
      Serial.print(cnt * len);
      Serial.println(" seconds");
      // Wait X seconds before retrying
      delay((cnt * len) * 1000);
      cnt = cnt * 2;
      if (cnt > 256) 
        cnt = 256;
    }
  }
}

void mqtt_publish(char *topic, char *payload) {
  int err;
  if (! client.publish(topic, payload)) {
    int rc  = client.state();
    if (rc < 0) {
      Serial.println("dead connection, retrying");
      reconnect();
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (havePir && state == INACTIVE) {
    if (turnedOn) 
      mqtt_publish(MQTT_TOPIC, "active");
    state = ACTIVE;
    Serial.println(" Motion Begin");
  }
  if (state == ACTIVE && xSemaphoreTake(timerSemaphore, 0) == pdTRUE) {
    if (havePir) {
      // read the sensor current value, 
      if (havePir && digitalRead(pirSensor)==LOW) {
        Serial.println("Motion continued PIR");
        havePir = false;
      }
      motionCount = delaySeconds;     // while motion, keep reseting countdown
    } else if (motionCount > 0) {
      motionCount--;
      if (motionCount == 0) {
        // publish to MQTT
        if (turnedOn) 
          mqtt_publish(MQTT_TOPIC, "inactive");
        state = INACTIVE;
        Serial.println(" Motion End");
      }
      else {
        Serial.print("countdown ");
        Serial.println(motionCount);
      }
    }
  }
}
