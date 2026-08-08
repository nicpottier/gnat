#pragma once

#include <Config.h>
#include <widget/Icons.h>
#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {

// a single adjustable value on an adjust page, values with names display and
// toggle through the named values with a single button instead of +/-
struct AdjustValue {
  const char* label;
  const char* unit;
  int step;
  int min;
  int max;
  int (*get)(Config&);
  void (*set)(Config&, int);
  const char* const* names;
};

// a swipeable page of up to two adjustable values with its own colors, an
// optional icon shown inline in the header band and optional help text
struct AdjustPage {
  const char* title;
  uint32_t headerBgColor;
  uint32_t headerTextColor;
  uint32_t bgColor;
  uint32_t textColor;
  uint32_t buttonColor;
  void (*icon)(TFT_eSPI&, int, int, uint32_t, uint32_t);
  const AdjustValue* values;
  int valueCount;
  const char* help;
};

static const AdjustValue water_adjust_values[] = {
    {"Warn", "mm", 1, min_warn_level, max_warn_level, [](Config& c) { return c.getWarnLevel(); },
     [](Config& c, int v) { c.setWarnLevel(v); }},
    {"Refill", "mm", 1, min_refill_level, max_refill_level, [](Config& c) { return c.getRefillLevel(); },
     [](Config& c, int v) { c.setRefillLevel(v); }},
};

static const AdjustValue espresso_adjust_values[] = {
    {"Stop", "g", 1, 1, max_stop_weight, [](Config& c) { return c.getStopWeight(); },
     [](Config& c, int v) { c.setStopWeight(v); }},
    {"Target", "s", 1, min_shot_target, max_shot_target, [](Config& c) { return c.getShotTarget(); },
     [](Config& c, int v) { c.setShotTarget(v); }},
};

static const char* direction_names[] = {"Left", "Right"};

static const AdjustValue grind_adjust_values[] = {
    {"Alert", "s", 1, min_shot_margin, max_shot_margin, [](Config& c) { return c.getShotMargin(); },
     [](Config& c, int v) { c.setShotMargin(v); }},
    {"Finer", "", 1, finer_direction_left, finer_direction_right, [](Config& c) { return c.getFinerDirection(); },
     [](Config& c, int v) { c.setFinerDirection(v); }, direction_names},
};

static const AdjustValue machine_adjust_values[] = {
    {"Sleep", "m", 5, 5, max_sleep_time, [](Config& c) { return c.getSleepTime(); },
     [](Config& c, int v) { c.setSleepTime(v); }},
    {"Flush", "s", 1, min_flush_seconds, max_flush_seconds, [](Config& c) { return c.getFlushSeconds(); },
     [](Config& c, int v) { c.setFlushSeconds(v); }},
};

static const char* orientation_names[] = {"Normal", "Flipped"};

static const AdjustValue app_adjust_values[] = {
    {"Flip", "", 1, orientation_normal, orientation_flipped, [](Config& c) { return c.getOrientation(); },
     [](Config& c, int v) { c.setOrientation(v); }, orientation_names},
};

// a coffee brown, a grind purple, a steel gray and a deep teal for these pages
const uint32_t espresso_page_color = 0x6A66;
const uint32_t grind_page_color = 0x7A77;
const uint32_t machine_page_color = 0x4A69;
const uint32_t app_page_color = 0x032C;

static const AdjustPage adjust_pages[] = {
    {"Water Levels", TFT_WHITE, theme.water_color, theme.water_color, TFT_WHITE, TFT_WHITE, drawDrop,
     water_adjust_values, 2},
    {"Espresso", TFT_WHITE, espresso_page_color, espresso_page_color, TFT_WHITE, TFT_WHITE, drawMug,
     espresso_adjust_values, 2},
    {"Grind Alerts", TFT_WHITE, grind_page_color, grind_page_color, TFT_WHITE, TFT_WHITE, drawGear,
     grind_adjust_values, 2},
    {"Machine", TFT_WHITE, machine_page_color, machine_page_color, TFT_WHITE, TFT_WHITE, drawPower,
     machine_adjust_values, 2},
    {"App", TFT_WHITE, app_page_color, app_page_color, TFT_WHITE, TFT_WHITE, drawSliders, app_adjust_values, 1,
     "Flipping orientation reboots your gnat"},
};
static const int adjust_page_count = sizeof(adjust_pages) / sizeof(adjust_pages[0]);

// button geometry in baseline design units, shared with the touch hit testing
const int adjust_button_r = 16;

// the header band height in design units
const int adjust_header_h = 40;

inline void adjustButtonCenter(int row, bool plus, int& cx, int& cy) {
  cx = px(plus ? 200 : 105);
  cy = px(70 + row * 40);
}

// the rect a toggle value's single wide button occupies, spanning from the
// minus button's left edge to the plus button's right edge
inline void adjustToggleRect(int row, int& x, int& y, int& w, int& h) {
  int cxMinus, cxPlus, cy;
  adjustButtonCenter(row, false, cxMinus, cy);
  adjustButtonCenter(row, true, cxPlus, cy);
  auto r = px(adjust_button_r);
  x = cxMinus - r;
  y = cy - r;
  w = (cxPlus + r) - x;
  h = r * 2;
}

// steps a value forward, wrapping, for toggle buttons
inline void adjustToggle(Config& config, const AdjustValue& value) {
  int v = value.get(config) + value.step;
  if (v > value.max) {
    v = value.min;
  }
  value.set(config, v);
}

// renders whichever adjust page is active with large +/- touch targets
class AdjustFields : public Widget {
 public:
  AdjustFields(int width, int height)
      : m_width{width},
        m_height{height} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) {
    if (ctx.adjustPage != m_page || ctx.config.getVersion() != m_version) {
      m_page = ctx.adjustPage;
      m_version = ctx.config.getVersion();
      m_config = ctx.config;
      return true;
    }
    return false;
  }

  void paint(TFT_eSPI& tft) {
    if (m_page < 0 || m_page >= adjust_page_count) {
      tft.fillRect(0, 0, m_width, m_height, theme.bg_color);
      return;
    }
    auto& page = adjust_pages[m_page];

    // header band with the icon inline before the title
    auto bandH = px(adjust_header_h);
    tft.fillRect(0, 0, m_width, bandH, page.headerBgColor);
    tft.fillRect(0, bandH, m_width, m_height - bandH, page.bgColor);

    tft.setFreeFont(FONT_TITLE);
    tft.setTextColor(page.headerTextColor, page.headerBgColor);
    auto textW = tft.textWidth(page.title);
    auto iconW = page.icon ? px(34) + px(8) : 0;
    auto startX = (m_width - textW - iconW) / 2;
    if (page.icon) {
      page.icon(tft, startX + px(17), bandH / 2, page.headerTextColor, page.headerBgColor);
    }
    tft.setTextDatum(ML_DATUM);
    tft.drawString(page.title, startX + iconW, bandH / 2);

    tft.setTextColor(page.textColor, page.bgColor);

    for (int i = 0; i < page.valueCount; i++) {
      auto& value = page.values[i];
      int cx, cy;

      adjustButtonCenter(i, false, cx, cy);
      tft.setFreeFont(FONT_BODY);
      tft.setTextDatum(CL_DATUM);
      tft.drawString(value.label, px(10), cy);

      if (value.names) {
        // named values render as one wide toggle button
        int bx, by, bw, bh;
        adjustToggleRect(i, bx, by, bw, bh);
        tft.fillRoundRect(bx, by, bw, bh, px(8), page.buttonColor);
        tft.setTextColor(page.bgColor, page.buttonColor);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(value.names[value.get(m_config) - value.min], bx + bw / 2, by + bh / 2);
        tft.setTextColor(page.textColor, page.bgColor);
        continue;
      }

      drawButton(tft, cx, cy, false, page);

      char buffer[16];
      snprintf(buffer, 16, "%d%s", value.get(m_config), value.unit);
      tft.setFreeFont(FONT_BODY);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(buffer, px(152), cy);

      adjustButtonCenter(i, true, cx, cy);
      drawButton(tft, cx, cy, true, page);
    }

    if (page.help) {
      tft.setFreeFont(FONT_BODY_SM);
      tft.setTextDatum(TC_DATUM);
      tft.drawString(page.help, m_width / 2, m_height - px(22));
    }

    tft.setTextDatum(TL_DATUM);
  }

 private:
  void drawButton(TFT_eSPI& tft, int cx, int cy, bool plus, const AdjustPage& page) {
    auto r = px(adjust_button_r);
    tft.fillCircle(cx, cy, r, page.buttonColor);
    tft.fillRect(cx - r / 2, cy - px(2), r, px(4), page.bgColor);
    if (plus) {
      tft.fillRect(cx - px(2), cy - r / 2, px(4), r, page.bgColor);
    }
  }

  Config m_config;
  int m_page = -1;
  unsigned long m_version = 0;
  int m_width;
  int m_height;
};

}  // namespace widget
