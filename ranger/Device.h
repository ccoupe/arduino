#ifndef MY_DEVICE_H
#define MY_DEVICE_H

#define WIFI_ID "CJCNET"
#define WIFI_PASSWORD "LostAgain2"

#define MQTT_SERVER "192.168.1.7"
#define MQTT_PORT 1883
#define MQTT_DEVICE "esp_trumpy_ranger"

// For Homie compatible topic structure
#define HDEVICE "trumpy_ranger"
#define HNAME "ESP32 in TrumpyBear"        // Display name is OK

//  HC-SR04 ultrasonic sensorPin numbers
#define echoPin 23
#define trigPin 4

// Define display - i2c OLED or undef for LCD 160x
//#define DISPLAY_U8

#ifdef DISPLAY_U8
#define DISPLAY_COLUMNS 8
#define DISPLAY_LINES 2
#else // LCD1602
#define LCD_I2C_ADDR 0x27
#define DISPLAY_COLUMNS 16
#define DISPLAY_LINES 2
#endif

/* 
 * Ranger modes
 * ONCE - signal once when object stop withing given distance (+/-) 
 * CONTINOUS - single whenever object within distance (multiple signals)
 * FREE - signal continously - makes for a very busy MQTT
 */
#define RGR_ONCE  0    
#define RGR_CONTINOUS 1
#define RGR_FREE  2
#define RGR_SNAP 3

#endif
