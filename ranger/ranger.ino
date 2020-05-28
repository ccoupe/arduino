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
#ifdef DISPLAY_U8
#include <U8x8lib.h>
U8X8_SH1106_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);
#else 
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

#endif

#define ACTIVE 1
#define INACTIVE 0
int state = INACTIVE;

int get_distance();
boolean dist_display(int d, int target_d);
void doDisplay(boolean state, char *argstr);

long lastMsg = 0;
char msg[50];
int value = 0;

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


// Called from mqtt with distance to measure towards
// anything over 400cm/13ft is too far to care about
// Beware - this can/will be called re-entrantly to cancel. Should protect rgr_running var
// and the ISR stuff but that's a lot work and testing for those failures is difficult.
uint32_t target_d = 0;
boolean rgr_running = false;
uint32_t next_pub = 1;
uint32_t next_tick = 0;

void doRanger(int dist) {
  int next_pub = 0;
  if (dist == 0) {   
    rgr_running = false;
    Serial.println("set to zero, stopping?");
    return;
  } else {
    target_d = dist;
    rgr_running = true;
    // don't run for more than a few minutes
    uint32_t tend = isrCounter + (1 * 60);
    int d = get_distance();
    Serial.println("Begin ranger");
    while (isrCounter < tend) {
      // check for canceled.
      if (rgr_running == false)
        break;
      if (d > 2 && d < 500)
        if (dist_display(d, target_d) == true) {
          wait_for_pub(d);
        }
      d = get_distance();
    }
    Serial.println("end DoRanger");
    if (isrCounter >= tend) {
      doDisplay(true,"Timed Out");
      mqtt_ranger_set_dist(0);
    }
  }
}

// Want to publish to mqtt every 5 seconds or (so)
// beware of the 'cleverness' in tick counting and what could go 
// wrong. Hint: there are many ways to fail
void wait_for_pub(int d) {
  if (next_tick == isrCounter) {
    return;    
  }
  next_pub -= 1;
  next_tick = isrCounter;
  if (next_pub <= 0) {
    mqtt_ranger_set_dist(d);
    next_pub = 5;
  }
}

#define ADJMIN 10
#define ADJMAX 10

boolean dist_display(int d, int target_d) {
  if (d > 400) {
    Serial.println("out of range");
    doDisplay(true, "Out of Range");
    return false;
  }
  if (d >= (target_d - ADJMIN) && d <= (target_d + ADJMAX)) {
    Serial.println("stop there");
    doDisplay(true, "Stop There");
    return true;
  }
  if (d < (target_d + ADJMAX)) {
    Serial.println("move back");
    doDisplay(true, "Move Back");
    return false;
  }
  if ( d > (target_d - ADJMIN)) {
    Serial.println("move forward");
    doDisplay(true, "Move Forward");
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
  mqtt_loop();
  delay(200);
  return (int)distance;
}

/* called from Mqtt and internally for commicating status
 *  ASSUMES 2 line display. Will fill words to fit available lines
 *  one and two words are special cases: map to line 1, or line1 and line 2
 *  Always centered. New line in imput str is treated as space.
*/

//   u8x8.setFont(u8x8_font_profont29_2x3_r); // only 8 chars across
//   u8x8.setFont(u8x8_font_inr21_2x4_f); // only 8 chars across
void doDisplay(boolean state, char *argstr) {
  /* if str == Null then turn display on/off based on state 
   * if str != Null then turn on and clear display
   * we always clear the display. str arg may have \n inside
   * to separate lines.
   */
   if (state == false) {
      Serial.println("clear display");
      //always clear 
#ifdef DISPLAY_U8
      u8x8.clear();
#else
      lcd.clear();
      lcd.noDisplay();
      lcd.noBacklight();
#endif
   } else {
#ifdef DISPLAY_U8
    u8x8.clear();
    u8x8.setFont(u8x8_font_profont29_2x3_r); // 8 chars across. 2 Lines
#else
    lcd.display();
    lcd.backlight();
    lcd.clear();
#endif
    if (argstr == (char *)0) {
      Serial.println("display on without string");
      return;
    }
    char *str;
    str = strdup(argstr);
    int len = strlen(str);
    char *words[8];
    int j = 0;
    words[0] = str;
    for (int i = 0; i < len; i++) {
      if (str[i] == '\n' || str[i] == ' ') {
        j++;
        words[j] = str+i+1;
        str[i] = '\0';
      }
    }
    if (j == 0) {
      // Only one word, center it (DISPLAY_COLUMNS max)
      int pos = 0;
      if (len < (DISPLAY_COLUMNS - 1)) 
        pos = (DISPLAY_COLUMNS - len) / 2;
#ifdef DISPLAY_U8
      u8x8.setCursor(pos*2,3);
      u8x8.print(words[0]);
#else
      lcd.setCursor(pos, 0);
      lcd.print(words[0]);
#endif
    } else {
      int lens[8];
      for (int i = 0; i <= j; i++) 
        lens[i] = strlen(words[i]);
      if (j == 1) {
        // two words, two lines
        for (int i = 0; i < 2; i++) {    
          int pos = 0;
          if (lens[i] < (DISPLAY_COLUMNS - 1)) 
            pos = (DISPLAY_COLUMNS - lens[i]) / 2;
#ifdef DISPLAY_U8
          u8x8.setCursor(pos*2,i*4);
          u8x8.print(words[i]);
#else
          lcd.setCursor(pos, i);
          lcd.print(words[i]);
#endif
        }
      } else {
        // pack words into line
        int nxt = fill_line(0, words, lens, 0, j+1, DISPLAY_COLUMNS);
        fill_line(1, words, lens, nxt, j+1, DISPLAY_COLUMNS);
      }
    }
  free(str);
  }
}  

int fill_line(int lnum, char **words, int wdlens[], int wdst, int maxw, int wid) {
  int len = 0;
  char ln[wid + 1] = {0};
  int pos = 0;
  for (int i = wdst; i < maxw; i++) {
    if ((len + wdlens[i]) <= wid) {
      strcat(ln, words[i]);
      strcat(ln, " ");
      len += (wdlens[i] + 1);
    } else {
      // would overflow, center, print and return
      if (len <= wid) 
         pos = (wid - len) / 2;
#ifdef DISPLAY_U8
       u8x8.setCursor(pos*2, lnum*4);
       u8x8.print(ln);
#else
       lcd.setCursor(pos, lnum);
       lcd.print(ln);
#endif
      return i;
    }
  }
  // if we get here, there is a partial line to print.
  if (len <= wid) 
     pos = (wid - len) / 2;
#ifdef DISPLAY_U8
  u8x8.setCursor(pos*2,lnum*4);
  u8x8.print(ln);
#else
  lcd.setCursor(pos, lnum);
  lcd.print(ln);
#endif
  return maxw;
}


#ifdef DISPLAY_U8
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
#endif

void setup() {
  Serial.begin(115200);
  Serial.println("Starting");

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);


#ifdef DISPLAY_U8
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
#else
  lcd.begin();
  lcd.display();
  lcd.backlight();
  lcd.print("Trumpy Bear");
#endif    
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
   
}
