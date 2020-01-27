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
#include <Wire.h>
#include "Device.h"


boolean turnedOn = true;  // controls whether device sends to MQTT - not used?
int state = INACTIVE;

hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;

volatile boolean havePir = false;

#define DelayToOff 45
unsigned int delaySeconds = DelayToOff;   // we want to set this variable from mqtt
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

void setup() {
  Serial.begin(115200);
  Serial.println("Starting");
  mqtt_setup(WIFI_ID, WIFI_PASSWORD, MQTT_SERVER, MQTT_PORT, MQTT_DEVICE,
      HDEVICE, HNAME, HPUB, HSUB);
  //client.setServer(mqtt_server, 1883);
  //client.setCallback(callback);
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
}

void loop() {
  mqtt_loop();
      
  if (havePir && state == INACTIVE) {
    if (turnedOn) {
#ifdef OLD
      mqtt_publish(MQTT_TOPIC, "active");
#else
      mqtt_homie_pub(HPUB, "active", false);
#endif
    }
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
        if (turnedOn) {
#ifdef OLD
          mqtt_publish(MQTT_TOPIC, "inactive");
#else
          mqtt_homie_pub(HPUB, "inactive", false);
#endif
        }
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
