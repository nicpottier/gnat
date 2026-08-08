#pragma once

#include <profiles.h>
#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {

class ProfileName : public Widget {
 public:
  ProfileName(int x, int y, int width)
      : m_x{x},
        m_y{y},
        m_width{width} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) {
    if (ctx.config.getProfile() != m_profile) {
      m_profile = ctx.config.getProfile();
      return true;
    }

    return false;
  }

  void paint(TFT_eSPI& tft) {
    tft.fillRect(m_x, m_y, m_width, px(14), theme.bg_color);

    if (m_profile < 1 || m_profile > profile_count) {
      return;
    }

    tft.setFreeFont(FONT_BODY_SM);
    tft.setTextColor(theme.text_color, theme.bg_color);
    tft.drawString(profiles[m_profile - 1].name, m_x, m_y);
  }

 private:
  int m_x;
  int m_y;
  int m_width;

  int m_profile = -1;
};

}  // namespace widget
