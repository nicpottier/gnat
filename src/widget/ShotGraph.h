#pragma once

#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {

class ShotFrame {
 public:
  double weight = 0;
  double groupPressure = 0;
  double headTemp = 0;
  double groupFlow = 0;
};

class ShotGraph : public Widget {
 public:
  ShotGraph(int x, int y) : m_x{x}, m_y{y} {};

  virtual bool tick(data::Context ctx, long tickID, long millis) {
    auto changed = false;
    if (ctx.getMachineBLEState() == BLEState::CONNECTED && ctx.getMachineBLEState() != m_bleState) {
      m_bleState = ctx.getMachineBLEState();
      changed = true;
    }

    if (ctx.machineState != m_state) {
      if (ctx.machineState == MachineState::espresso) {
        m_clear = true;
        changed = true;
        m_frame = 0;
      }
      m_state = ctx.machineState;
    }

    if (ctx.lastSample.sampleTime != m_lastSample) {
      m_frame = (m_frame + 1) % 240;
      auto sample = ctx.lastSample;
      m_frames[m_frame].weight = ctx.currentWeight;
      m_frames[m_frame].groupPressure = sample.groupPressure;
      m_frames[m_frame].headTemp = sample.headTemp;
      m_frames[m_frame].groupFlow = sample.groupFlow;
      m_frames[m_frame].weight = ctx.currentWeight;
      m_lastSample = ctx.lastSample.sampleTime;
      changed = true;
    }

    return changed;
  }

  virtual void paint(TFT_eSPI &tft) {
    // clear our graph if appropriate
    if (m_clear) {
      tft.fillRect(m_x, m_y, 240, 100, COLOR_BG);
      m_clear = false;
    }

    // draw our current frame
    int x = m_x + m_frame;

    // clear ahead of us
    tft.drawRect(x, m_y, 20, 100, COLOR_BG);

    // if we are near the edge, clear there as well
    if (x + 20 > 240) {
      auto wrap = (x + 20) % 240;
      tft.drawRect(0, m_y, wrap, 100, COLOR_BG);
    }

    // draw our temp
    int y = m_y + 100 - ((m_frames[m_frame].headTemp - 85) * 10) - 30;
    tft.drawLine(x, y, x, y + 2, COLOR_ERROR);

    // draw our flow
    y = m_y + 100 - m_frames[m_frame].groupFlow * 4 - 30;
    tft.drawLine(x, y, x, y + 2, COLOR_WATER);

    // draw our pressure
    y = m_y + 100 - m_frames[m_frame].groupPressure * 2 - 50;
    tft.drawLine(x, y, x, y + 2, COLOR_PRESSURE);

    // draw our weight
    y = m_y + 100 - m_frames[m_frame].weight * 2 - 8;
    tft.drawLine(x, y, x, y + 2, COLOR_WEIGHT);
  }

 private:
  int m_x;
  int m_y;
  BLEState m_bleState = BLEState::UNKNOWN;
  MachineState m_state = MachineState::unknown;
  int m_lastSample = 0;
  int m_frame = 0;
  bool m_clear = false;
  ShotFrame m_frames[240];
};

}  // namespace widget
