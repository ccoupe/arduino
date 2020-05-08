/*********
 * Use rcwl-0516 and a AM312 as motion sensor for MQTT
 * The RCWL is very sensitive and far ranging, and 360 degree
 * the AM312 is less of those attributes. We use it to to start
 * a detection and the rcwl will extend the detection .
 
*********/
#include <Arduino.h>
#include <Wire.h>
#include "Device.h"
#include "MQTT_Motion.h"
#include <EEPROM.h>
#define ACTIVE 1
#define INACTIVE 0
int state = INACTIVE;
#define EEPROM_SIZE 2                     // a short int

long lastMsg = 0;
char msg[50];
int value = 0;

hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;


// RCWL-0516 Microwave motion sensor
//const int mwSensor = 17;
volatile boolean haveMw = false;      // set by ISR

// AM312 pir motion sensor
//#define  pirSensor 16
volatile boolean havePir = false;

//#define DelayToOff 25
unsigned int delaySeconds = DelayToOff;   // this variable can be set dynamically
int motionCount = 0;

// Interrupt handler for motion on AM312 pir
void IRAM_ATTR intrPir() {
  //Serial.println("PIR ISR");
  havePir = true;
}

//Interrupt handler for motion on RCWL-0516 pir
void IRAM_ATTR detectsMovement() {
  //Serial.println("RCWL ISR");
  haveMw = true;
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
  
  mqtt_setup(WIFI_ID, WIFI_PASSWORD, MQTT_SERVER, MQTT_PORT, MQTT_DEVICE,
      HDEVICE, HNAME, delay_get, delay_set);

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

  //motionSemaphore = xSemaphoreCreateBinary();
  // PIR Motion Sensor mode INPUT_PULLUP
  pinMode(pirSensor, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pirSensor), intrPir, RISING);
  
  // Microwave Motion Sensor mode INPUT_PULLUP
  pinMode(mwSensor, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(mwSensor), detectsMovement, RISING);

}

void loop() {
   mqtt_loop();

  if (haveMw && havePir && state == INACTIVE) {
    mqtt_homie_active(true); 
    state = ACTIVE;
    Serial.println(" Motion Begin");
  }
  if (state == ACTIVE && xSemaphoreTake(timerSemaphore, 0) == pdTRUE) {
    if (haveMw || havePir) {
      // read the sensor current value, 
      if (haveMw && digitalRead(mwSensor)==LOW) {
        Serial.println("Motion continued RCWL");
        haveMw = false;
      }
      if (havePir && digitalRead(pirSensor)==LOW) {
        Serial.println("Motion continued PIR");
        havePir = false;
      }
      motionCount = delaySeconds;     // while motion, keep reseting countdown
    } else if (motionCount > 0) {
      motionCount--;
      if (motionCount == 0) {
        mqtt_homie_active(false); 
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
