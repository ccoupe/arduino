#ifndef MY_DEVICE_H
#define MY_DEVICE_H

#define WIFI_ID "CJCNET"
#define WIFI_PASSWORD "LostAgain2"

#define MQTT_SERVER "192.168.1.7"
#define MQTT_PORT 1883
#define MQTT_DEVICE "esp_garage_ranger"

// For Homie compatible topic structure
#define HDEVICE "audi_stall"
#define HNAME "ESP32 Ranger"        // Display name is OK

//  HC-SR04 ultrasonic sensorPin numbers
#define echoPin 23
#define trigPin 4

#define USE_FLASH
#define ACTIVE 1
#define INACTIVE 0

/* 
 * Ranger modes
 * ONCE - signal once when object stop withing given distance (+/-) 
 * CONTINOUS - single whenever object within distance (multiple signals)
 * FREE - signal continously - makes for a very busy MQTT

#define RGR_ONCE  0    
#define RGR_CONTINOUS 1
#define RGR_FREE  2
#define RGR_SNAP 3
 */
#endif
