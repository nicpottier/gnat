#pragma once

#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {

// a full tank reads roughly this many mm above the sensor
const int water_level_full_mm = 40;

class WaterLevel : public Widget {
 public:
  WaterLevel(int x, int y, int width, int height)
      : m_x{x},
        m_y{y},
        m_width{width},
        m_height{height} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) {
    auto level = ctx.waterLevel;

    // while the machine is moving water the sensor sloshes around, only let
    // the level fall, once at rest track the real reading again
    if (m_level >= 0 && machineMovingWater(ctx.machineState)) {
      level = min(m_level, level);
    }

    if (level != m_level || ctx.waterLevelThreshold != m_threshold ||
        ctx.config.getWarnLevel() != m_warnLevel) {
      m_level = level;
      m_threshold = ctx.waterLevelThreshold;
      m_warnLevel = ctx.config.getWarnLevel();
      return true;
    }

    return false;
  }

  void paint(TFT_eSPI& tft) {
    // a thicker border with softly rounded corners
    auto bt = max(2, px(2));
    auto r = px(4);
    for (int i = 0; i < bt; i++) {
      tft.drawRoundRect(m_x + i, m_y + i, m_width - i * 2, m_height - i * 2, max(1, r - i), theme.water_color);
    }

    // the interior of our bar
    auto ix = m_x + bt + 1;
    auto iy = m_y + bt + 1;
    auto iw = m_width - (bt + 1) * 2;
    auto ih = m_height - (bt + 1) * 2;

    tft.fillRect(ix, iy, iw, ih, theme.bg_color);

    auto levelPx = ih * min(m_level, water_level_full_mm) / water_level_full_mm;
    auto thresholdPx = ih * min(m_threshold, water_level_full_mm) / water_level_full_mm;
    auto warnPx = ih * min(m_warnLevel, water_level_full_mm) / water_level_full_mm;

    // the fill below the machine's refill threshold gets its own color
    auto lowPx = min(levelPx, thresholdPx);
    if (lowPx > 0) {
      tft.fillRect(ix, iy + ih - lowPx, iw, lowPx, theme.water_low_color);
    }

    // between the refill threshold and our warning level is the warning zone
    auto warnFillPx = min(levelPx, max(warnPx, thresholdPx));
    if (warnFillPx > lowPx) {
      tft.fillRect(ix, iy + ih - warnFillPx, iw, warnFillPx - lowPx, theme.water_warn_color);
    }

    // the rest is regular water
    if (levelPx > warnFillPx) {
      tft.fillRect(ix, iy + ih - levelPx, iw, levelPx - warnFillPx, theme.water_color);
    }
  }

 private:
  int m_x;
  int m_y;
  int m_width;
  int m_height;

  int m_level = -1;
  int m_threshold = -1;
  int m_warnLevel = -1;
};

}  // namespace widget
