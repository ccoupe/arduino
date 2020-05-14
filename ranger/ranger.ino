/*
 * Mqtt-Ranger is an HR-504 ultrasonic sensor to meausre distance in cm.
 * and a HiLetgo 1.3" i2c OLED LCD. Two different 'nodes' on once device
 * in the Homie MQTT world. 
 * Measuring can 'take over' the display so they are not independent
*/
 

#include <Arduino.h>
#include <Wire.h>
#include "Device.h"
#include "MQTT_Ranger.h"
#include <U8x8lib.h>
U8X8_SH1106_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);

#define ACTIVE 1
#define INACTIVE 0
int state = INACTIVE;

long lastMsg = 0;
char msg[50];
int value = 0;

hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;


// Interrupt handler for motion on AM312 pir
void IRAM_ATTR intrPir() {
  //Serial.println("PIR ISR");  havePir = true;
}

//Interrupt handler for motion on RCWL-0516 pir
void IRAM_ATTR detectsMovement() {
  //Serial.println("RCWL ISR");
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


// Called from mqtt with distance to measure towards
// anything over 400cm/13ft is too far to care about
int target_d = 0;

void doRanger(int dist) {
  if (dist == 0) {
    // turn off display
    target_d = dist;
  } else {
    target_d = dist;
    // turn on display
    // some kind of timer - don't run for more than a few minutes
    int tend = lastIsrAt + (2 * 60);
    int d = get_distance();
    while (dist_display(d, target_d) && (lastIsrAt < tend)) {
      d = get_distance();
    }
    if (lastIsrAt >= tend) {
      // timed out
    } else {
      mqtt_ranger_set_dist(0);
    }
    mqtt_ranger_set_dist(d);
  }
}

boolean dist_display(int d, int target_d) {
  if (d > 400) {
    // display "Walk forward to 3 ft"
    Serial.println("Walk forward to 3 ft");
    return false;
  }
  if (d >= (target_d - 5) && d <= (target_d + 5)) {
    // display "stop there"
    Serial.println("Stop there");
    return true;
  }
  if (d < target_d) {
    // display "backwards a little bit"
    Serial.println("backwards a little bit");
    return false;
  }
  if ( d > target_d) {
    // display "forward a little more"
    Serial.println("forward a little more");
    return false;
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
  delay(100);
  return (int)distance;
}

// called from Mqtt and internally. 
//   u8x8.setFont(u8x8_font_profont29_2x3_r); // only 8 chars across
//   u8x8.setFont(u8x8_font_inr21_2x4_f); // only 8 chars across
void doDisplay(boolean state, char *str) {
  /* if str == Null then turn display on/off based on state 
   * if str != Null then turn on and clear display
   * we always clear the display. str arg may have \n inside
   * to separate lines.
   */
   if (str == (char *)0l) {
     u8x8.clear();
   } else {
    //Serial.println("doDisplay called");
    u8x8.clear();
    u8x8.setFont(u8x8_font_profont29_2x3_r); // 8 chars across. 2 Lines
    int len = strlen(str);
    char *ln[2];
    int j = 0;
    ln[0] = str;
    for (int i = 0; i < len; i++) {
      if (str[i] == '\n') {
        j++;
        ln[j] = str+i+1;
        str[i] = '\0';
      }
    }
    if (j == 0) {
      // Only one line, center it (8 max)
      Serial.println("one line");
      int pos = 0;
      if (len < 7) 
        pos = (8 - len) / 2;
      u8x8.setCursor(pos*2,3);
      u8x8.print(ln[0]);
    } else {
      for (int i = 0; i <= j; i++) {
        //Serial.println(ln[i]);
        len = strlen(ln[i]);
        int pos = 0;
        if (len < 7) 
          pos = (8 - len) / 2;
        u8x8.setCursor(pos*2,i*4);
        u8x8.print(ln[i]);
      }
    }
  }
}

const uint8_t colLow = 4;
const uint8_t colHigh = 13;
const uint8_t rowCups = 0;
const uint8_t rowState = 2; // Double spacing the Rows
const uint8_t rowTemp = 4; // Double spacing the Rows
const uint8_t rowTime = 6; // Double spacing the Rows

void lcdBold(bool aVal) {
  if (aVal) {
    u8x8.setFont(u8x8_font_victoriabold8_r); // BOLD
  } else {
    u8x8.setFont(u8x8_font_victoriamedium8_r); // NORMAL
  }
}


void setup() {
  Serial.begin(115200);
  Serial.println("Starting");
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  u8x8.begin();
  lcdBold(true); // You MUST make this call here to set a Font
  // u8x8.setPowerSave(0); // Too lazy to find out if I need this
  u8x8.clear();
  u8x8.setCursor(0,rowCups);
  u8x8.print(F("Trumpy Bear"));
  u8x8.setCursor(0,rowState);
  u8x8.print(__DATE__);
  u8x8.setCursor(0,rowTemp);
  u8x8.print(__TIME__);
    
  mqtt_setup(WIFI_ID, WIFI_PASSWORD, MQTT_SERVER, MQTT_PORT, MQTT_DEVICE,
      HDEVICE, HNAME, doRanger, doDisplay);

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
   mqtt_loop();
   //doRanger(75);  //test
   //int d = get_distance();
   
}
