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
  BLEState scaleBLEState = BLEState::unknown;
};

// how many samples of history we keep, bounds the drawable width
const int shot_graph_capacity = 480;

class ShotGraph : public Widget {
 public:
  ShotGraph(int x, int y, int width, int height) : m_x{x}, m_y{y}, m_width{width}, m_height{height} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) override {
    auto changed = false;
    if (ctx.getMachineBLEState() == BLEState::connected && ctx.getMachineBLEState() != m_machineBLEState) {
      m_machineBLEState = ctx.getMachineBLEState();
      changed = true;
    }

    // starting a new shot clears our history
    if (ctx.machineState == MachineState::espresso && ctx.machineSubstate != m_substate) {
      if (m_substate < MachineSubstate::preinfusing && ctx.machineSubstate >= MachineSubstate::preinfusing) {
        m_count = 0;
        changed = true;
      }
    }
    m_state = ctx.machineState;
    m_substate = ctx.machineSubstate;

    // we only record samples from preinfusion through the end of the pour, the
    // last shot stays on display until the next one starts
    if (ctx.machineState == MachineState::espresso && ctx.machineSubstate >= MachineSubstate::preinfusing &&
        ctx.machineSubstate <= MachineSubstate::ending && ctx.lastSample.sampleTime != m_lastSample) {
      m_head = (m_head + 1) % shot_graph_capacity;
      auto sample = ctx.lastSample;
      m_frames[m_head].weight = ctx.currentWeight;
      m_frames[m_head].groupPressure = sample.groupPressure;
      m_frames[m_head].headTemp = sample.headTemp;
      m_frames[m_head].groupFlow = sample.groupFlow;
      m_frames[m_head].scaleBLEState = ctx.getScaleBLEState();
      m_lastSample = ctx.lastSample.sampleTime;

      if (m_count < maxSamples()) {
        m_count++;
      }
      changed = true;
    }

    return changed;
  }

  void paint(TFT_eSPI &tft) override {
#ifdef DISPLAY_RM67162
    // we already render into a full screen framebuffer, drawing directly is
    // flicker free and avoids nesting sprites
    m_spriteFailed = true;
#endif

    // render into a sprite so scrolling doesn't flicker, fall back to
    // drawing directly if we can't get the memory for one
    if (!m_sprite && !m_spriteFailed) {
      m_sprite = new TFT_eSprite(&tft);
      if (m_sprite->createSprite(m_width, m_height) == nullptr) {
        delete m_sprite;
        m_sprite = nullptr;
        m_spriteFailed = true;
      }
    }

    if (m_sprite && !m_spriteFailed) {
      m_sprite->fillSprite(theme.bg_color);
      drawFrames(*m_sprite, 0, 0);
      m_sprite->drawRoundRect(0, 0, m_width, m_height, 10, theme.dash_border_color);
      m_sprite->pushSprite(m_x, m_y);
    } else {
      tft.fillRect(m_x, m_y, m_width, m_height, theme.bg_color);
      drawFrames(tft, m_x, m_y);
      tft.drawRoundRect(m_x, m_y, m_width, m_height, 10, theme.dash_border_color);
    }
  }

 private:
  // how many samples fit in our plot area
  int maxSamples() {
    return min(m_width - 4, shot_graph_capacity);
  }

  // draws our history oldest to newest as connected lines, newest hugging the
  // right edge once we've filled the width so the graph scrolls left
  void drawFrames(TFT_eSPI &gfx, int x0, int y0) {
    int prevFlow = -1;
    int prevPressure = -1;
    int prevWeight = -1;

    for (int i = 0; i < m_count; i++) {
      auto &frame = m_frames[(m_head - m_count + 1 + i + shot_graph_capacity) % shot_graph_capacity];
      int x = x0 + 2 + i;

      int flow = valueY(y0, frame.groupFlow, 0, 10, 45);
      plotValue(gfx, x, flow, prevFlow, theme.water_color);
      prevFlow = flow;

      int pressure = valueY(y0, frame.groupPressure, 0, 12, 29);
      plotValue(gfx, x, pressure, prevPressure, theme.pressure_color);
      prevPressure = pressure;

      // weight only plots for samples taken with the scale connected
      if (frame.scaleBLEState == BLEState::connected) {
        int weight = valueY(y0, frame.weight, 0, 50, 15);
        plotValue(gfx, x, weight, prevWeight, theme.weight_color);
        prevWeight = weight;
      } else {
        prevWeight = -1;
      }
    }
  }

  // connects the previous sample to this one so fast changes leave no gaps
  void plotValue(TFT_eSPI &gfx, int x, int y, int prevY, uint32_t color) {
    if (prevY < 0) {
      gfx.drawPixel(x, y, color);
    } else {
      gfx.drawLine(x - 1, prevY, x, y, color);
    }
  }

  // scales a value to a y position, clamped inside our plot area
  int valueY(int y0, double value, int min, int max, int offset) {
    auto scaled = (value - min) / (double)(max - min) * (m_height - offset - 2);
    int y = m_height - scaled - offset;

    if (y < 2) {
      y = 2;
    } else if (y > m_height - 2) {
      y = m_height - 2;
    }
    return y0 + y;
  }

  int m_x;
  int m_y;
  int m_width;
  int m_height;

  BLEState m_machineBLEState = BLEState::unknown;

  MachineState m_state = MachineState::unknown;
  MachineSubstate m_substate = MachineSubstate::unknown;

  int m_lastSample = 0;

  // ring buffer of samples, m_head is the newest, m_count how many are valid
  int m_head = 0;
  int m_count = 0;
  ShotFrame m_frames[shot_graph_capacity];

  TFT_eSprite *m_sprite = nullptr;
  bool m_spriteFailed = false;
};

}  // namespace widget
