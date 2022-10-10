#pragma once

#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {
class MachineStatus : public Widget {
 public:
  MachineStatus(int x, int y) : m_x{x}, m_y{y} {};

  virtual bool tick(data::Context ctx, long tickID, long millis) {
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

  virtual void paint(TFT_eSPI &tft) {
    tft.drawCircle(m_x + 10, m_y + 10, 8, COLOR_BG);
    if (m_lastState == BLEState::CONNECTED) {
      tft.fillCircle(m_x + 10, m_y + 10, 7, COLOR_BLE);
    } else {
      auto color = (m_lastState == BLEState::CONNECTING) ? COLOR_BLE : COLOR_ERROR;
      if (m_flash) {
        tft.fillCircle(m_x + 10, m_y + 10, 7, color);
      } else {
        tft.fillCircle(m_x + 10, m_y + 10, 7, COLOR_DASH_BG);
      }
    }

    tft.fillRect(m_x + 22, m_y, 80, 20, COLOR_DASH_BG);

    if (m_lastState == BLEState::CONNECTED && m_headTemp > 0) {
      char buffer[10];
      snprintf(buffer, 10, "%dC", m_headTemp);

      tft.setFreeFont(&FreeMonoBold9pt7b);
      tft.setTextColor(TFT_WHITE, COLOR_DASH_BG);
      tft.drawString(buffer, m_x + 23, m_y + 4);
    } else {
      tft.setFreeFont(&FreeMono9pt7b);
      tft.setTextColor(TFT_WHITE, COLOR_DASH_BG);
      tft.drawString("no de1", m_x + 23, m_y + 4);
    }
  }

 private:
  BLEState m_lastState = BLEState::UNKNOWN;
  int m_x;
  int m_y;
  bool m_flash = true;
  int m_headTemp = 0;
};

}  // namespace widget
