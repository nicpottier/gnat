#pragma once

#include <widget/Theme.h>
#include <widget/Widget.h>

// TODO: this is a dupe of main content, move to separate include
#define STRINGIZER(arg) #arg
#define STR_VALUE(arg) STRINGIZER(arg)
#define SPLASH_VERSION STR_VALUE(BUILD_VERSION)

namespace widget {
class ConfigBackground : public Widget {
 public:
  ConfigBackground(int width, int height)
      : m_width{width},
        m_height{height} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) {
    // only redraw when forced by screen
    return false;
  }

  void paint(TFT_eSPI& tft) {
    tft.fillScreen(theme.bg_color);
    tft.fillRoundRect(0, 0, m_width, 35, 10, theme.dash_bg_color);
    tft.fillRect(0, 10, m_width, 25, theme.dash_bg_color);
    tft.drawRect(0, 0, m_width, 35, theme.dash_border_color);
    tft.fillRoundRect(0, 35, m_width, m_height - 35, 10, theme.dash_bg_color);
    tft.fillRect(0, 35, m_width, 50, theme.dash_bg_color);
    tft.fillRoundRect(3, 38, m_width - 6, m_height - 35 - 6, 10, theme.bg_color);

    tft.setFreeFont(&FreeSans9pt7b);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(theme.text_color, theme.dash_bg_color);
    tft.drawString("Configuration", 10, 10);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(SPLASH_VERSION, m_width - 10, 10);
    tft.setTextDatum(TL_DATUM);
  }

 private:
  int m_width;
  int m_height;
};

}  // namespace widget