#pragma once

#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {
class ShotTimer : public Widget {
 public:
  ShotTimer(int x, int y, int width) : m_x{x}, m_y{y}, m_width{width} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) {
    bool changed = false;

    // we just started a pour, log the start time
    if (ctx.machineState == MachineState::espresso && ctx.machineSubstate != m_substate &&
        ctx.machineSubstate == MachineSubstate::pouring) {
      m_shotStart = millis;
      m_shotTime = 0;
      changed = true;
    }

    // shot ended, save our final time
    if (m_state == MachineState::espresso && m_substate == MachineSubstate::pouring &&
        ctx.machineSubstate != MachineSubstate::pouring) {
      m_shotTime = millis - m_shotStart;
      changed = true;
    }

    m_state = ctx.machineState;
    m_substate = ctx.machineSubstate;

    // when brewing force a repaint every 5th tick (~250ms)
    if (m_state == MachineState::espresso && tickID % 5 == 0) {
      changed = true;
    }
    return changed;
  }

  void paint(TFT_eSPI& tft) {
    tft.fillRect(m_x, m_y, m_width, 22, theme.dash_bg_color);

    // if we have a shot time, show that
    if (m_shotTime > 0) {
      auto elapsed = m_shotTime / (double)1000;
      char buffer[10];
      snprintf(buffer, 10, "%0.1fs", elapsed);
      tft.setFreeFont(&FreeSans9pt7b);
      tft.setTextColor(TFT_WHITE, theme.dash_bg_color);
      tft.drawString(buffer, m_x + 10, m_y + 4);
    }

    // otherwise, if we are brewing and have a start, show elapsed
    else if (m_state == MachineState::espresso && m_shotStart > 0) {
      auto elapsed = (millis() - m_shotStart) / (double)1000;
      char buffer[10];
      snprintf(buffer, 10, "%0.1fs", elapsed);
      tft.setFreeFont(&FreeSans9pt7b);
      tft.setTextColor(TFT_WHITE, theme.dash_bg_color);
      tft.drawString(buffer, m_x + 10, m_y + 4);
    }
  }

 private:
  int m_x;
  int m_y;
  int m_width;

  unsigned long m_shotStart = 0;
  unsigned long m_shotTime = 0;

  MachineState m_state = MachineState::unknown;
  MachineSubstate m_substate = MachineSubstate::unknown;
};

}  // namespace widget