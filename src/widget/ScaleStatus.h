#pragma once

#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {
class ScaleStatus : public Widget {
 public:
  ScaleStatus(int x, int y, int width)
      : m_x{x},
        m_y{y},
        m_width{width} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) {
    bool changed = false;
    auto weight = int(ctx.currentWeight * 10) / double(10);

    if (m_lastWeight != weight) {
      m_lastWeight = weight;
      changed = true;
    }

    if (ctx.getScaleBLEState() != m_lastState) {
      m_lastState = ctx.getScaleBLEState();
      changed = true;
    }

    // if we aren't connected, figure out if we need to flash
    if (m_lastState != BLEState::connected) {
      if (tickID % 20 == 0) {
        m_flash = !m_flash;
        changed = true;
      }
    }

    return changed;
  }

  void paint(TFT_eSPI &tft) {
    tft.drawCircle(m_x + px(8), m_y + px(8), px(6), theme.dash_border_color);
    if (m_lastState == BLEState::connected) {
      tft.fillCircle(m_x + px(8), m_y + px(8), px(5), theme.ble_color);
    } else {
      auto color = (m_lastState == BLEState::connecting) ? theme.ble_color : theme.error_color;
      if (m_flash) {
        tft.fillCircle(m_x + px(8), m_y + px(8), px(5), color);
      } else {
        tft.fillCircle(m_x + px(8), m_y + px(8), px(5), theme.dash_bg_color);
      }
    }

    // draw our weight if connected
    tft.fillRect(m_x + px(18), m_y, m_width - px(18), px(18), theme.dash_bg_color);

    if (m_lastState == BLEState::connected) {
      char buffer[10];
      if (m_lastWeight > 0 && m_lastWeight < 100) {
        snprintf(buffer, 10, "%0.1fg", m_lastWeight);
      } else {
        snprintf(buffer, 10, "%0.0fg", m_lastWeight);
      }

      tft.setFreeFont(FONT_BODY_SM);
      tft.setTextColor(theme.text_color, theme.dash_bg_color);
      tft.drawString(buffer, m_x + px(19), m_y + px(2));
    } else {
      tft.setFreeFont(FONT_BODY_SM);
      tft.setTextColor(theme.text_color, theme.dash_bg_color);
      tft.drawString("Scale", m_x + px(19), m_y + px(2));
    }
  }

 private:
  BLEState m_lastState = BLEState::unknown;

  int m_x;
  int m_y;
  int m_width;

  double m_lastWeight = 0;
  bool m_flash = true;
};

}  // namespace widget
