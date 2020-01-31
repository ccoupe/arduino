#ifndef MY_DEVICE_H
#define MY_DEVICE_H

#define WIFI_ID "CJCNET"
#define WIFI_PASSWORD "LostAgain2"

#define MQTT_SERVER "192.168.1.7"
#define MQTT_PORT 1883
#define MQTT_DEVICE "ESP32_Keystudio1"

// For Homie compatible topic structure
#define HDEVICE "family_motion1"
#define HNAME "KeyStudio PIR"         // Display name is OK
//#define HPUB "homie/"HDEVICE"/motionsensor/motion"
//#define HSUB "homie/"HDEVICE"/motionsensor/active_hold/set"
//#define HSUBQ "homie/"HDEVICE"/motionsensor/active_hold"

// Keystudio pir motion sensor. Pin number
#define pirSensor 23
#define DelayToOff 45
#define USE_FLASH 


#endif
