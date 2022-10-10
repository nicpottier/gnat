#pragma once

#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {
class ScaleStatus : public Widget {
 public:
  ScaleStatus(int x, int y) : m_x{x}, m_y{y}, m_lastWeight{0} {};

  virtual bool tick(data::Context ctx, long tickID, long millis) {
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
    if (m_lastState != BLEState::CONNECTED) {
      if (tickID % 20 == 0) {
        m_flash = !m_flash;
        changed = true;
      }
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

    // draw our weight if connected
    tft.fillRect(m_x + 22, m_y, 90, 22, COLOR_DASH_BG);

    if (m_lastState == BLEState::CONNECTED) {
      char buffer[10];
      if (m_lastWeight < 100) {
        snprintf(buffer, 10, "%0.1fg", m_lastWeight);
      } else {
        snprintf(buffer, 10, "%0.0fg", m_lastWeight);
      }

      tft.setFreeFont(&FreeMonoBold9pt7b);
      tft.setTextColor(TFT_WHITE, COLOR_DASH_BG);
      tft.drawString(buffer, m_x + 23, m_y + 4);
    } else {
      tft.setFreeFont(&FreeMono9pt7b);
      tft.setTextColor(TFT_WHITE, COLOR_DASH_BG);
      tft.drawString("no scale", m_x + 23, m_y + 4);
    }
  }

 private:
  BLEState m_lastState = BLEState::UNKNOWN;
  int m_x;
  int m_y;
  double m_lastWeight;
  bool m_flash = true;
};

}  // namespace widget
