#ifndef MY_DEVICE_H
#define MY_DEVICE_H
//extern const char* ssid ;
//extern const char* password;
//extern const char* mqtt_server;
#define WIFI_USER "CJCNET"
#define WIFI_PASSWORD "LostAgain2"

#define MQTT_SERVER "192.168.1.7"
#define MQTT_PORT 1883
#define MQTT_DEVICE "ESP32_Keystudio1"
#define MQTT_TOPIC "sensors/family/keystudio1"
#define MQTT_CMD   "sensors/family/keystudio1_control"

#define HDEVICE "family_motion_1"
#define HDNAME "$name"
#define HDNODES "$nodes"
#define HNODE "motion"
#define HNODENAME "$name"
#define HNODETYPE "$type" // motionsensor
#define HNODEPROP "$properties" 

// Keystudio pir motion sensor
#define  pirSensor 23
#define ACTIVE 1
#define INACTIVE 0

// these variables are defined in the driver and shared with mqtt_lib
extern int state;
extern boolean turnedOn;      // controls whether device sends to MQTT
extern unsigned int delaySeconds;

// Functions in mqtt_lib.cpp 

extern void mqtt_setup();
extern void mqtt_reconnect();
extern void mqtt_loop();
extern void mqtt_send_config();
extern void mqtt_callback(char* topic, byte* payload, unsigned int length);
extern void mqtt_publish(char *topic, char *payload);


#endif
