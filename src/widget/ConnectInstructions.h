#pragma once

#include <widget/Theme.h>
#include <widget/Widget.h>
#include <widget/splash_sprite.h>

namespace widget {
class ConnectInstructions : public Widget {
 public:
  ConnectInstructions(int width, int height)
      : m_width{width},
        m_height{height} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) {
    // only redraw when forced by screen
    return false;
  }

  void paint(TFT_eSPI& tft) {
    tft.fillScreen(theme.bg_color);
    tft.fillRoundRect(0, 0, m_width, m_height, 10, theme.dash_bg_color);
    tft.fillRoundRect(5, 5, m_width - 10, m_height - 10, 10, theme.bg_color);

    auto centerX = m_width / 2;

    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(theme.text_color, theme.bg_color);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Connect to", centerX, m_height / 2 - 50);

    // TODO: investigate replacing with drawBitmap which would be way smaller
    tft.pushImage((m_width - 159) / 2, (m_height - 55) / 2, splash_sprite_width, splash_sprite_height, splash_sprite);

    tft.drawString("WIFI AP to configure", centerX, m_height / 2 + 35);

    tft.setTextDatum(TL_DATUM);
  }

 private:
  int m_width;
  int m_height;
};

}  // namespace widget