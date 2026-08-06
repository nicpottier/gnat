#pragma once

#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {

const int status_height = 35;

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
    auto sh = px(status_height);
    auto r = px(10);
    tft.fillRect(0, 0, m_width, m_height, theme.bg_color);
    tft.fillRoundRect(0, 0, m_width, sh, r, theme.dash_bg_color);
    tft.fillRect(0, r, m_width, sh - r, theme.dash_bg_color);
    tft.drawRect(0, 0, m_width, sh, theme.dash_border_color);
    tft.fillRoundRect(0, sh, m_width, m_height - sh, r, theme.dash_bg_color);
    tft.fillRect(0, sh, m_width, px(50), theme.dash_bg_color);
    tft.fillRoundRect(px(3), sh + px(3), m_width - px(6), m_height - sh - px(6), r, theme.bg_color);
  }

 private:
  int m_width;
  int m_height;
};

}  // namespace widget