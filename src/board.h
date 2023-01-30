#pragma once
#include <TFT_eSPI.h>

const uint8_t BACKLIGHT_ON = 100;
const int BACKLIGHT_PWM_FREQ = 10000;
const int BACKLIGHT_PWM_RESOLUTION = 8;
const int BACKLIGHT_PWM_CHANNEL = 0;

void turnOffDisplay(TFT_eSPI tft) {
#ifdef M5_STICK
  M5.Axp.ScreenSwitch(false);
#else
  ledcWrite(BACKLIGHT_PWM_CHANNEL, 0);
  tft.writecommand(ST7789_DISPOFF);
#endif
}

void turnOnDisplay(TFT_eSPI tft) {
#ifdef M5_STICK
  M5.Axp.ScreenSwitch(true);
#else
  ledcWrite(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_ON);
  tft.writecommand(ST7789_DISPON);
#endif
}
