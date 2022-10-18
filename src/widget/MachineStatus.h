#pragma once

#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {
class MachineStatus : public Widget {
 public:
  MachineStatus(int x, int y, int width) : m_x{x}, m_y{y}, m_width{width} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) {
    bool changed = false;

    if (ctx.getMachineBLEState() != m_lastState) {
      m_lastState = ctx.getMachineBLEState();
      changed = true;
    }

    auto headTemp = int(ctx.lastSample.headTemp);
    if (headTemp != m_headTemp) {
      m_headTemp = headTemp;
      changed = true;
    }

    if (tickID % 20 == 0) {
      m_flash = !m_flash;
      changed = true;
    }

    return changed;
  }

  void paint(TFT_eSPI &tft) {
    tft.drawCircle(m_x + 10, m_y + 10, 8, theme.dash_border_color);
    if (m_lastState == BLEState::connected) {
      tft.fillCircle(m_x + 10, m_y + 10, 7, theme.ble_color);
    } else {
      auto color = (m_lastState == BLEState::connecting) ? theme.ble_color : theme.error_color;
      if (m_flash) {
        tft.fillCircle(m_x + 10, m_y + 10, 7, color);
      } else {
        tft.fillCircle(m_x + 10, m_y + 10, 7, theme.dash_bg_color);
      }
    }

    tft.fillRect(m_x + 22, m_y, m_width - 22, 20, theme.dash_bg_color);

    if (m_lastState == BLEState::connected && m_headTemp > 0) {
      char buffer[10];
      snprintf(buffer, 10, "%dC", m_headTemp);

      tft.setFreeFont(&FreeSans9pt7b);
      tft.setTextColor(TFT_WHITE, theme.dash_bg_color);
      tft.drawString(buffer, m_x + 23, m_y + 4);
    } else {
      tft.setFreeFont(&FreeSans9pt7b);
      tft.setTextColor(TFT_WHITE, theme.dash_bg_color);
      tft.drawString("DE1", m_x + 23, m_y + 4);
    }
  }

 private:
  BLEState m_lastState = BLEState::unknown;
  int m_x;
  int m_y;
  int m_width;

  bool m_flash = true;
  int m_headTemp = 0;
};

}  // namespace widget
