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
    auto sh = px(35);
    auto r = px(10);
    tft.fillRect(0, 0, m_width, m_height, theme.bg_color);
    tft.fillRoundRect(0, 0, m_width, sh, r, theme.dash_bg_color);
    tft.fillRect(0, r, m_width, sh - r, theme.dash_bg_color);
    tft.drawRect(0, 0, m_width, sh, theme.dash_border_color);
    tft.fillRoundRect(0, sh, m_width, m_height - sh, r, theme.dash_bg_color);
    tft.fillRect(0, sh, m_width, px(50), theme.dash_bg_color);
    tft.fillRoundRect(px(3), sh + px(3), m_width - px(6), m_height - sh - px(6), r, theme.bg_color);

    tft.setFreeFont(FONT_BODY);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(theme.text_color, theme.dash_bg_color);
    tft.drawString("Configuration", px(10), px(10));
    tft.setTextDatum(TR_DATUM);
    tft.drawString(SPLASH_VERSION, m_width - px(10), px(10));
    tft.setTextDatum(TL_DATUM);
  }

 private:
  int m_width;
  int m_height;
};

}  // namespace widget