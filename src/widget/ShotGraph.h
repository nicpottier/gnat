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
  BLEState scaleBLEState = BLEState::UNKNOWN;
};

class ShotGraph : public Widget {
 public:
  ShotGraph(int x, int y, int width, int height) : m_x{x}, m_y{y}, m_width{width}, m_height{height} {};

  virtual bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) override {
    auto changed = false;
    if (ctx.getMachineBLEState() == BLEState::CONNECTED && ctx.getMachineBLEState() != m_machineBLEState) {
      m_machineBLEState = ctx.getMachineBLEState();
      changed = true;
    }

    if (ctx.machineState == MachineState::espresso && ctx.machineSubstate != m_substate) {
      if (m_substate < MachineSubstate::preinfusion && ctx.machineSubstate >= MachineSubstate::preinfusion) {
        m_clear = true;
        m_frame = 10;
        changed = true;
      }
    }
    m_state = ctx.machineState;
    m_substate = ctx.machineSubstate;

    if (ctx.lastSample.sampleTime != m_lastSample) {
      m_frame = (m_frame + 1) % (m_width);
      auto sample = ctx.lastSample;
      m_frames[m_frame].weight = ctx.currentWeight;
      m_frames[m_frame].groupPressure = sample.groupPressure;
      m_frames[m_frame].headTemp = sample.headTemp;
      m_frames[m_frame].groupFlow = sample.groupFlow;
      m_frames[m_frame].weight = ctx.currentWeight;
      m_frames[m_frame].scaleBLEState = ctx.getScaleBLEState();
      m_lastSample = ctx.lastSample.sampleTime;
      changed = true;
    }

    return changed;
  }

  void plotValue(TFT_eSPI &tft, uint32_t color, double value, int min, int max, int offset) {
    int x = (m_frame % m_width) + m_x;

    // figure out our y
    auto scaled = (value - min) / (double)(max - min) * (m_height - offset - 2);
    int y = m_y + m_height - scaled - offset;

    // don't draw out of bounds
    if (y - 2 < m_y || y - 2 > m_y + m_height) {
      return;
    }
    tft.drawLine(x, y, x, y - 2, color);
  }

  virtual void paint(TFT_eSPI &tft) override {
    // clear our graph if appropriate
    if (m_clear) {
      tft.fillRoundRect(m_x - 1, m_y - 1, m_width + 2, m_height + 2, 10, COLOR_BG);
      m_clear = false;
    }

    // draw our current frame
    int x = (m_frame % m_width) + m_x;

    // clear ahead of us
    tft.drawRect(x, m_y, min(x + 20, m_x + m_width) - x, m_height, COLOR_BG);

    // if we are near the edge, clear there as well
    if (x + 20 > m_width) {
      auto wrap = (x + 20) % m_width;
      tft.drawRect(m_x, m_y, wrap, m_height, COLOR_BG);
    }

    // draw our sample data if the machine is connected
    if (m_machineBLEState == BLEState::CONNECTED) {
      // draw our flow
      plotValue(tft, COLOR_WATER, m_frames[m_frame].groupFlow, 0, 10, 45);

      // draw our pressure
      plotValue(tft, COLOR_PRESSURE, m_frames[m_frame].groupPressure, 0, 12, 29);
    }

    // draw our weight if we have a scale connected
    if (m_frames[m_frame].scaleBLEState == BLEState::CONNECTED) {
      plotValue(tft, COLOR_WEIGHT, m_frames[m_frame].weight, 0, 50, 15);
    }

    // draw our border
    tft.drawRoundRect(m_x, m_y, m_width, m_height, 10, COLOR_DASH_LINE);
    tft.drawRoundRect(m_x - 1, m_y - 1, m_width + 2, m_height + 2, 10, COLOR_DASH_BG);
  }

 private:
  int m_x;
  int m_y;
  int m_width;
  int m_height;

  BLEState m_machineBLEState = BLEState::UNKNOWN;
  BLEState m_scaleBLEState = BLEState::UNKNOWN;

  MachineState m_state = MachineState::unknown;
  MachineSubstate m_substate = MachineSubstate::unknown;

  int m_lastSample = 0;
  int m_frame = 0;

  bool m_clear = false;

  ShotFrame m_frames[240];
};

}  // namespace widget
