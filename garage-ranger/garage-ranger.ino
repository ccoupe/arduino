/*
 * Garage-Ranger is an HR-504 ultrasonic sensor to meausre distance in cm.
 * It communicates with MQTT. Psuedo Homie structure,
 * Listen on  homie/audi_stall/ranger/distance/set for json payload:
 *   {"period": secs}
 *  That 16bit integer value is written into NVRAM and read from there on startup
 * The sensor is read every 'period' seconds, If it's changed +/- 1cm then
 *    The distance in cm is published to 'homie/audi_stall/ranger/distance'
 * Each 4 hours (14400 secs  we publish anyway.
 * 
 * That's not 'homie' it only looks like it.
 * There is a matching Hubitat driver.
*/
 

#include <Arduino.h>
#include <Wire.h>
#include "Device.h"
#include "MQTT_Ranger.h"
#include <EEPROM.h>
#define EEPROM_SIZE 2

int state = INACTIVE;

#define DelayToOff 5
unsigned int delaySeconds = DelayToOff;   // this variable can be set dynamically
unsigned countDown = DelayToOff;
unsigned fourHours = 60*60+4;
int last_d;

// forward declare
int get_distance();

hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;

void IRAM_ATTR onTimer(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  isrCounter++;
  lastIsrAt = millis();
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
}

uint32_t target_d = 0;
boolean rgr_running = false;
uint32_t next_pub = 25;
uint32_t next_tick = 0;

// Mqtt callback - parse json string the hacky way
void setRanger(String jstr) {
  if (jstr.startsWith("{\"period\"")) {
    int c = jstr.indexOf(":");
    jstr.remove(0,c+1);
    jstr.trim();
    int s = jstr.toInt();
    if (s > 0) {
      delay_set(s);
    }
    

  }
  
}

void delay_set(int val) {
  // don't update flash unless value changes
  if (val != delaySeconds) {
    delaySeconds = val;
#ifdef USE_FLASH
    //update_flash(val);
    EEPROM.write(0, (delaySeconds & 0xFF00));
    EEPROM.write(1, (delaySeconds & 0x00FF));
    EEPROM.commit();
#endif  
    Serial.print("set delaySeconds to ");
    Serial.println(delaySeconds);
  } else {
    Serial.println("skipping flash writing");
  }
}

int get_distance() {
  float duration, distance;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = (duration*.0343)/2;
  Serial.print("Distance: ");
  Serial.println(distance);
  mqtt_loop();
  delay(200);
  return (int)distance;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting");
#ifdef USE_FLASH
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
#endif  

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  mqtt_setup(WIFI_ID, WIFI_PASSWORD, MQTT_SERVER, MQTT_PORT, MQTT_DEVICE,
      HDEVICE, HNAME, setRanger);

  // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();

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

}

void loop() {
  if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE) {
    fourHours--;
    if (fourHours <= 0) {
      fourHours = 60*60+4;
      int d = get_distance();
      last_d = d;
      mqtt_ranger_set_dist(d);
    } else {
      countDown--;
      if (countDown <= 0) {
        // time to take a measurement
        int d = get_distance();
        if (d < (last_d - 1) || d >= (last_d + 1)) {
          // publish distance
          last_d = d;
          mqtt_ranger_set_dist(d);
        }
        // reset countdown timer
        countDown = delaySeconds;
      }
    }
  }
  mqtt_loop();

   
}
