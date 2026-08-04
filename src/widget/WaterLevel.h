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
    if (ctx.waterLevel != m_level || ctx.waterLevelThreshold != m_threshold) {
      m_level = ctx.waterLevel;
      m_threshold = ctx.waterLevelThreshold;
      return true;
    }

    return false;
  }

  void paint(TFT_eSPI& tft) {
    // the interior of our bar
    auto ix = m_x + 2;
    auto iy = m_y + 2;
    auto iw = m_width - 4;
    auto ih = m_height - 4;

    tft.fillRect(ix, iy, iw, ih, theme.bg_color);
    tft.drawRoundRect(m_x, m_y, m_width, m_height, 2, theme.water_color);

    auto levelPx = ih * min(m_level, water_level_full_mm) / water_level_full_mm;
    auto thresholdPx = ih * min(m_threshold, water_level_full_mm) / water_level_full_mm;

    // the fill below the refill threshold gets its own color
    auto lowPx = min(levelPx, thresholdPx);
    if (lowPx > 0) {
      tft.fillRect(ix, iy + ih - lowPx, iw, lowPx, theme.water_low_color);
    }

    // the rest is regular water
    if (levelPx > thresholdPx) {
      tft.fillRect(ix, iy + ih - levelPx, iw, levelPx - thresholdPx, theme.water_color);
    }
  }

 private:
  int m_x;
  int m_y;
  int m_width;
  int m_height;

  int m_level = -1;
  int m_threshold = -1;
};

}  // namespace widget
