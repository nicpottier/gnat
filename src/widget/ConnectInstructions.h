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
    tft.fillRect(0, 0, m_width, m_height, theme.bg_color);
    tft.fillRoundRect(0, 0, m_width, m_height, px(10), theme.dash_bg_color);
    tft.fillRoundRect(px(5), px(5), m_width - px(10), m_height - px(10), px(10), theme.bg_color);

    auto centerX = m_width / 2;

    tft.setFreeFont(FONT_BODY);
    tft.setTextColor(theme.text_color, theme.bg_color);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Connect to", centerX, m_height / 2 - px(50));

    // TODO: investigate replacing with drawBitmap which would be way smaller
    // pushImage isn't virtual, when rendering into a framebuffer sprite we have
    // to call the sprite's version so it doesn't touch the (unused) tft bus
#ifdef DISPLAY_RM67162
    ((TFT_eSprite&)tft).pushImage((m_width - 159) / 2, (m_height - 55) / 2, splash_sprite_width, splash_sprite_height,
                                  (uint16_t*)splash_sprite);
#else
    tft.pushImage((m_width - 159) / 2, (m_height - 55) / 2, splash_sprite_width, splash_sprite_height, splash_sprite);
#endif

    tft.drawString("WIFI AP to configure", centerX, m_height / 2 + px(35));

    tft.setTextDatum(TL_DATUM);
  }

 private:
  int m_width;
  int m_height;
};

}  // namespace widget