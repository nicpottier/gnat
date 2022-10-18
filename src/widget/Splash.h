#pragma once

#include <widget/Theme.h>
#include <widget/Widget.h>
#include <widget/splash_sprite.h>

// TODO: this is a dupe of main content, move to separate include
#define STRINGIZER(arg) #arg
#define STR_VALUE(arg) STRINGIZER(arg)
#define SPLASH_VERSION STR_VALUE(BUILD_VERSION)

namespace widget {
class Splash : public Widget {
 public:
  Splash(int width, int height)
      : m_width{width},
        m_height{height} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) {
    // only redraw when forced by screen
    return false;
  }

  void paint(TFT_eSPI& tft) {
    Serial.println("paint!");
    tft.fillScreen(theme.bg_color);
    tft.setFreeFont(&FreeMono9pt7b);
    tft.setTextColor(TFT_WHITE, theme.bg_color);
    tft.drawString(SPLASH_VERSION, 199 - strlen(SPLASH_VERSION) * 11 - 8, 80);

    tft.pushImage(40, 20, 159, 55, splash_sprite);
  }

 private:
  int m_width;
  int m_height;
};

}  // namespace widget