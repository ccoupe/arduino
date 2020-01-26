
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
  // Create and Publish the infrastructure topics for Homie 
  char *node = "homie/"HDEVICE"/$name";
  Serial.println(node);
  client.publish(node, "keystudio1");
  client.publish("homie/"HDEVICE"/$bodes", "sensor");
}

void mqtt_send_config() {
  char *tmpl = "conf={\"active_hold\":%d}";
  char msg[50];
  sprintf(msg, tmpl, delaySeconds);
  Serial.print("sending ");
  Serial.println(msg);
  mqtt_publish(MQTT_TOPIC, "active");
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
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

void mqtt_loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

}
