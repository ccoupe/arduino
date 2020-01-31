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
#include <EEPROM.h>
#include "MQTT_Motion.h"
#include "Device.h"

#define ACTIVE 1
#define INACTIVE 0

boolean turnedOn = true;  // controls whether device sends to MQTT - not used?
int state = INACTIVE;

hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;

volatile boolean havePir = false;

unsigned int delaySeconds = DelayToOff;   // we can set this variable from mqtt.
                                          // it's also shadowed from Flash memory
#define EEPROM_SIZE 2                     // a short int

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

int delay_get() {
  return delaySeconds;
}
void delay_set(int val) {
  // don't update flash unless value changes
  if (val != delaySeconds) {
    delaySeconds = val;
    //update_flash(val);
    EEPROM.write(0, (delaySeconds & 0xFF00));
    EEPROM.write(1, (delaySeconds & 0x00FF));
    EEPROM.commit();
  
    Serial.print("set delaySeconds to ");
    Serial.println(delaySeconds);
  } else {
    Serial.println("skipping flash writing");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting");
  
  EEPROM.begin(EEPROM_SIZE);
  byte h,l = 0x0FF;
  h = EEPROM.read(0);
  l = EEPROM.read(1);
  if (h == 255 && l == 255) {
    // uninitialized
    Serial.println("one time flash init");
    EEPROM.write(0, (delaySeconds & 0xFF00));
    EEPROM.write(1, (delaySeconds & 0x00FF));
    EEPROM.commit();
  } else {
    delaySeconds = (h << 8) | l;
  }
  
  Serial.print("EEPROM: ");
  Serial.print(h); Serial.print(" "); Serial.println(l);
  
  mqtt_setup(WIFI_ID, WIFI_PASSWORD, MQTT_SERVER, MQTT_PORT, MQTT_DEVICE,
      HDEVICE, HNAME, delay_get, delay_set);

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
      mqtt_homie_active(true); 
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
        if (turnedOn) {
          mqtt_homie_active(false); 
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
