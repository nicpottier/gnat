#pragma once

#include <profiles.h>
#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {

class ShotFrame {
 public:
  double weight = 0;
  double groupPressure = 0;
  double headTemp = 0;
  double groupFlow = 0;
  double targetPressure = 0;
  double targetFlow = 0;
  uint8_t frameNumber = 0;
  BLEState scaleBLEState = BLEState::unknown;
};

// how many samples of history we keep, bounds the drawable width
const int shot_graph_capacity = 480;

// how thick the graph traces draw, in baseline design units
const uint32_t shot_graph_bg = theme.bg_color;
const int shot_graph_line = 4;

// the machine's target pressure and flow draw underneath the actual traces
// as thin dimmed lines so you can see how well the profile is tracking
const uint32_t shot_graph_target_pressure = 0x2243;
const uint32_t shot_graph_target_flow = 0x000F;

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
      m_frames[m_head].targetPressure = sample.targetGroupPressure;
      m_frames[m_head].targetFlow = sample.targetGroupFlow;
      m_frames[m_head].frameNumber = sample.frameNumber;
      m_frames[m_head].scaleBLEState = ctx.getScaleBLEState();
      m_lastSample = ctx.lastSample.sampleTime;
      m_profile = ctx.config.getProfile();

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
      m_sprite->fillSprite(shot_graph_bg);
      drawFrames(*m_sprite, 0, 0);
      drawFrameNumber(*m_sprite, 0, 0);
      m_sprite->drawRoundRect(0, 0, m_width, m_height, px(10), theme.dash_border_color);
      m_sprite->pushSprite(m_x, m_y);
    } else {
      tft.fillRect(m_x, m_y, m_width, m_height, shot_graph_bg);
      drawFrames(tft, m_x, m_y);
      drawFrameNumber(tft, m_x, m_y);
      tft.drawRoundRect(m_x, m_y, m_width, m_height, px(10), theme.dash_border_color);
    }
  }

 private:
  // how many samples fit in our plot area, samples advance by the ui scale
  // so a shot covers the same physical width on any panel
  int maxSamples() {
    return min((m_width - px(4)) * 100 / UI_SCALE_PCT, shot_graph_capacity);
  }

  // draws our history oldest to newest as connected lines, newest hugging the
  // right edge once we've filled the width so the graph scrolls left, targets
  // draw first so the actual traces sit on top of them
  void drawFrames(TFT_eSPI &gfx, int x0, int y0) {
    int prevX = -1;
    int prevTargetFlow = -1;
    int prevTargetPressure = -1;

    for (int i = 0; i < m_count; i++) {
      auto &frame = m_frames[(m_head - m_count + 1 + i + shot_graph_capacity) % shot_graph_capacity];
      int x = x0 + px(2) + i * UI_SCALE_PCT / 100;

      int targetFlow = valueY(y0, frame.targetFlow, 0, 10, 45);
      plotThin(gfx, x, targetFlow, prevX, prevTargetFlow, shot_graph_target_flow);
      prevTargetFlow = targetFlow;

      int targetPressure = valueY(y0, frame.targetPressure, 0, 12, 29);
      plotThin(gfx, x, targetPressure, prevX, prevTargetPressure, shot_graph_target_pressure);
      prevTargetPressure = targetPressure;

      prevX = x;
    }

    prevX = -1;
    int prevFlow = -1;
    int prevPressure = -1;
    int prevWeight = -1;

    for (int i = 0; i < m_count; i++) {
      auto &frame = m_frames[(m_head - m_count + 1 + i + shot_graph_capacity) % shot_graph_capacity];
      int x = x0 + px(2) + i * UI_SCALE_PCT / 100;

      int flow = valueY(y0, frame.groupFlow, 0, 10, 45);
      plotValue(gfx, x, flow, prevX, prevFlow, theme.water_color);
      prevFlow = flow;

      int pressure = valueY(y0, frame.groupPressure, 0, 12, 29);
      plotValue(gfx, x, pressure, prevX, prevPressure, theme.pressure_color);
      prevPressure = pressure;

      // weight only plots for samples taken with the scale connected
      if (frame.scaleBLEState == BLEState::connected) {
        int weight = valueY(y0, frame.weight, 0, 50, 15);
        plotValue(gfx, x, weight, prevX, prevWeight, theme.weight_color);
        prevWeight = weight;
      } else {
        prevWeight = -1;
      }

      prevX = x;
    }
  }

  // during a shot, shows which profile frame the machine is executing
  void drawFrameNumber(TFT_eSPI &gfx, int x0, int y0) {
    if (m_state != MachineState::espresso || m_count == 0 || m_profile < 1 || m_profile > profile_count) {
      return;
    }

    char buffer[10];
    snprintf(buffer, 10, "%d/%d", m_frames[m_head].frameNumber + 1, profiles[m_profile - 1].frameCount);
    gfx.setFreeFont(FONT_BODY_SM);
    gfx.setTextColor(theme.text_color, shot_graph_bg);
    gfx.setTextDatum(TR_DATUM);
    gfx.drawString(buffer, x0 + m_width - px(10), y0 + px(6));
    gfx.setTextDatum(TL_DATUM);
  }

  // a single thin line for target traces
  void plotThin(TFT_eSPI &gfx, int x, int y, int prevX, int prevY, uint32_t color) {
    if (prevY < 0 || prevX < 0) {
      gfx.drawPixel(x, y, color);
    } else {
      gfx.drawLine(prevX, prevY, x, y, color);
    }
  }

  // connects the previous sample to this one so fast changes leave no gaps,
  // stacking offset lines to reach our thickness
  void plotValue(TFT_eSPI &gfx, int x, int y, int prevX, int prevY, uint32_t color) {
    auto thick = max(2, px(shot_graph_line));
    for (int t = 0; t < thick; t++) {
      auto dy = t - thick / 2;
      if (prevY < 0 || prevX < 0) {
        gfx.drawPixel(x, y + dy, color);
      } else {
        gfx.drawLine(prevX, prevY + dy, x, y + dy, color);
      }
    }
  }

  // scales a value to a y position, clamped inside our plot area, offsets are
  // in baseline design units
  int valueY(int y0, double value, int min, int max, int offset) {
    auto scaled = (value - min) / (double)(max - min) * (m_height - px(offset) - px(2));
    int y = m_height - scaled - px(offset);

    if (y < px(2)) {
      y = px(2);
    } else if (y > m_height - px(2)) {
      y = m_height - px(2);
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

  // the selected profile when the last sample landed, for the frame count
  int m_profile = 0;

  // ring buffer of samples, m_head is the newest, m_count how many are valid
  int m_head = 0;
  int m_count = 0;
  ShotFrame m_frames[shot_graph_capacity];

  TFT_eSprite *m_sprite = nullptr;
  bool m_spriteFailed = false;
};

}  // namespace widget
