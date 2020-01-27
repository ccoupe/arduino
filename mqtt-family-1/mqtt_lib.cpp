
// One day, this might be a class. For now, it's just C
/*
 * Handle all of the mqtt interactions
 */
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Arduino_JSON.h>
#include "Device.h"

//const char* ssid = WIFI_USER;
//const char* password = WIFI_PASSWORD;
const char* mqtt_server = MQTT_SERVER;
char *homie_pub = HPUB;
WiFiClient espClient;
PubSubClient client(espClient);

static void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_USER);

  WiFi.begin(WIFI_USER, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void mqtt_setup() {
  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqtt_callback);
  mqtt_reconnect();
#ifndef OLD  
  // Create and Publish the infrastructure topics for Homie v3
  mqtt_homie_pub("homie/"HDEVICE"/$homie", "3.0", true);
  mqtt_homie_pub("homie/"HDEVICE"/$name", HNAME, true);
  mqtt_homie_pub("homie/"HDEVICE"/$nodes", "sensor", true);
  mqtt_homie_pub("homie/"HDEVICE"/sensor/$name", "motion detector", true);
  mqtt_homie_pub("homie/"HDEVICE"/sensor/$type", "motion", true);
  mqtt_homie_pub("homie/"HDEVICE"/sensor/$properties", "motion,active_hold", true);
  // Property 'motion'
  mqtt_homie_pub("homie/"HDEVICE"/sensor/motion/$name", "Motion State", true);
  mqtt_homie_pub("homie/"HDEVICE"/sensor/motion/$datatype", "string", true);
  mqtt_homie_pub("homie/"HDEVICE"/sensor/motion/$settable", "false", true);
  // Property 'active_hold'
  mqtt_homie_pub("homie/"HDEVICE"/sensor/active_hold/$name", "Active Hold", true);  
  mqtt_homie_pub("homie/"HDEVICE"/sensor/active_hold/$datatype", "integer", true);
  mqtt_homie_pub("homie/"HDEVICE"/sensor/active_hold/$settable", "true", true);
#endif
  
}

#ifdef OLD
void mqtt_send_config() {
  char *tmpl = "conf={\"active_hold\":%d}";
  char msg[50];
  sprintf(msg, tmpl, delaySeconds);
  Serial.print("sending ");
  Serial.println(msg);
  mqtt_publish(MQTT_TOPIC, "active");
}
#endif

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". payload: ");
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
#ifndef OLD
  if (String(topic).equals(HSUB)) {
    // payload should be a string of digits 
    int d = messageTemp.toInt();
    if (d < 5)
       d = 5;
    else if (d > 3600)
       d = 3600;
    delaySeconds = d;
    Serial.print("set delaySeconds to ");
    Serial.println(delaySeconds);
  } 
#else
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
#endif
}

void mqtt_reconnect() {
  // Loop until we're reconnected
  int cnt = 1;
  int len = 5;
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_DEVICE)) {
      Serial.println("connected");
      // Subscribe
#ifdef OLD
      client.subscribe(MQTT_CMD);
#else
      client.subscribe(HSUB);
#endif
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

#ifdef OLD
void mqtt_publish(char *topic, char *payload) {
  int err;
  if (! client.publish(topic, payload)) {
    int rc  = client.state();
    if (rc < 0) {
      Serial.print(rc);
      Serial.println(" dead connection, retrying");
      mqtt_reconnect();
    }
  }
}
#endif

void mqtt_homie_pub(char *topic, char *payload, bool retain) {
   int err;
  if (! client.publish(topic, payload, retain)) {
    int rc  = client.state();
    if (rc < 0) {
      Serial.print(rc);
      Serial.println(" dead connection, retrying");
      mqtt_reconnect();
    }
  }
 
}

// Called from sketches loop()
void mqtt_loop() {
  if (!client.connected()) {
    mqtt_reconnect();
  }
  client.loop();

}
