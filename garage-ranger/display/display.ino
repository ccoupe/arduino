//
// OLED Test
//
#include <U8x8lib.h>
U8X8_SH1106_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);

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

void lcdRun() {
  lcdBold(false);
  u8x8.clear(); // ("0123456789012345")
  u8x8.setCursor(0,rowCups); u8x8.print(F("Cup: Run:OFF"));
  u8x8.setCursor(0,rowState); u8x8.print(F("Now:STOPPED "));
  u8x8.setCursor(0,rowTemp); u8x8.print(F("Tmp: Rly: "));
  u8x8.setCursor(0,rowTime); u8x8.print(F("Tim: Min: "));
}


void setup() {
  Serial.begin(115200);
  u8x8.begin();
  lcdBold(true); // You MUST make this call here to set a Font
  // u8x8.setPowerSave(0); // Too lazy to find out if I need this
  u8x8.clear();
  u8x8.setCursor(0,rowCups);
  u8x8.print(F("RICE COOKER"));
  u8x8.setCursor(0,rowState);
  u8x8.print(__DATE__);
  u8x8.setCursor(0,rowTemp);
  u8x8.print(__TIME__);
  delay(3000);
  lcdRun();
}

void loop() {
}
