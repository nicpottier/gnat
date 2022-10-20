#pragma once

#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {
class BrewBackground : public Widget {
 public:
  BrewBackground(int width, int height)
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
    tft.fillRoundRect(0, 35, m_width, 100, 10, theme.dash_bg_color);
    tft.fillRect(0, 35, m_width, 50, theme.dash_bg_color);
    tft.fillRoundRect(3, 38, m_width - 6, 94, 10, theme.bg_color);
  }

 private:
  int m_width;
  int m_height;
};

}  // namespace widget