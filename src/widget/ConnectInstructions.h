#pragma once

#include <widget/Logo.h>
#include <widget/Theme.h>
#include <widget/Widget.h>

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
    tft.fillRect(0, 0, m_width, m_height, theme.bg_color);
    tft.fillRoundRect(0, 0, m_width, m_height, px(10), theme.dash_bg_color);
    tft.fillRoundRect(px(5), px(5), m_width - px(10), m_height - px(10), px(10), theme.bg_color);

    auto centerX = m_width / 2;

    tft.setFreeFont(FONT_BODY);
    tft.setTextColor(theme.text_color, theme.bg_color);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Connect to", centerX, m_height / 2 - px(50));

    // TODO: investigate replacing with drawBitmap which would be way smaller
    drawLogo(tft, (m_width - splash_sprite_width) / 2, (m_height - splash_sprite_height) / 2, 1);

    tft.drawString("WIFI AP to configure", centerX, m_height / 2 + px(35));

    tft.setTextDatum(TL_DATUM);
  }

 private:
  int m_width;
  int m_height;
};

}  // namespace widget