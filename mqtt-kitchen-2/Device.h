#ifndef MY_DEVICE_H
#define MY_DEVICE_H

#define WIFI_ID "CJCNET"
#define WIFI_PASSWORD "LostAgain2"

#define MQTT_SERVER "192.168.1.7"
#define MQTT_PORT 1883
#define MQTT_DEVICE "ESP32_Kitchen2"

// For Homie compatible topic structure
#define HDEVICE "kitchen_motion2"
#define HNAME "Kitchen Motion Combo"        // Display name is OK

//  Pin numbers
#define pirSensor 4
#define mwSensor 5
#define DelayToOff 25
#define USE_FLASH 


#endif
