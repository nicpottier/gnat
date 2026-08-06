#pragma once

#include <Config.h>
#include <widget/Theme.h>
#include <widget/Widget.h>

// TODO: this is a dupe of main content, move to separate include
#define STRINGIZER(arg) #arg
#define STR_VALUE(arg) STRINGIZER(arg)
#define SPLASH_VERSION STR_VALUE(BUILD_VERSION)

namespace widget {
class ConfigFields : public Widget {
 public:
  ConfigFields(int x, int y, int width, int height)
      : m_x{x},
        m_y{y},
        m_width{width},
        m_height{height} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) {
    if (ctx.config.getVersion() != m_config.getVersion()) {
      m_config = ctx.config;
      return true;
    }

    return false;
  }

  void paint(TFT_eSPI& tft) {
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(theme.text_color, theme.bg_color);

    char buffer[16];
    snprintf(buffer, 16, "%dg", m_config.getStopWeight());
    tft.drawString("Stop weight:", m_x, m_y);
    tft.drawString(buffer, m_x + 128, m_y);

    snprintf(buffer, 16, "%d min", m_config.getSleepTime());
    tft.drawString("Sleep time:", m_x, m_y + 18);
    tft.drawString(buffer, m_x + 128, m_y + 18);

    // refill and warning levels share a line
    snprintf(buffer, 16, "%d / %d mm", m_config.getRefillLevel(), m_config.getWarnLevel());
    tft.drawString("Water levels:", m_x, m_y + 36);
    tft.drawString(buffer, m_x + 128, m_y + 36);

    snprintf(buffer, 16, "%ds", m_config.getShotMargin());
    tft.drawString("Grind margin:", m_x, m_y + 54);
    tft.drawString(buffer, m_x + 128, m_y + 54);

    snprintf(buffer, 16, "%d of %d", m_config.enabledProfileCount(), profile_count);
    tft.drawString("Profiles:", m_x, m_y + 72);
    tft.drawString(buffer, m_x + 128, m_y + 72);
  }

 private:
  Config m_config;
  int m_x;
  int m_y;
  int m_width;
  int m_height;
};

}  // namespace widget