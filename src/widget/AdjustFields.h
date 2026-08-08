#pragma once

#include <Config.h>
#include <profiles.h>
#include <widget/Icons.h>
#include <widget/Theme.h>
#include <widget/Units.h>
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

  // temperature values store celsius and display per the units setting
  bool temp;
};

// what kind of page this is: standard value rows, the custom profile
// cycler, or the custom hot water preset page
enum class AdjustPageKind : uint8_t { values, profile, hot_water };

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

  // centered pages stack the label over a centered control instead of the
  // label left / control right layout
  bool centered;

  AdjustPageKind kind;
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

static const AdjustValue steam_adjust_values[] = {
    {"Temp", "C", 1, min_steam_temp, max_steam_temp, [](Config& c) { return c.getSteamTemp(); },
     [](Config& c, int v) { c.setSteamTemp(v); }, nullptr, true},
    {"Time", "s", 10, min_steam_seconds, max_steam_seconds, [](Config& c) { return c.getSteamSeconds(); },
     [](Config& c, int v) { c.setSteamSeconds(v); }},
};

// the hot water page toggles between named presets rather than raw numbers,
// tea temperatures on the first row and common cup sizes on the second
struct HotWaterPreset {
  const char* name;
  int value;
};

static const HotWaterPreset hot_water_teas[] = {
    {"Delicate Tea", 71}, {"Green Tea", 79},    {"White Tea", 85},
    {"Oolong Tea", 91},   {"French Press", 93}, {"Black & Herbal Teas", 96},
};
static const int hot_water_tea_count = sizeof(hot_water_teas) / sizeof(hot_water_teas[0]);

// the machine's volume field is a single byte, bigger presets pour in 250ml
// rounds, one press of the hot water button each
static const int hot_water_pour_max = 250;

static const HotWaterPreset hot_water_sizes[] = {
    {"Demitasse", 90}, {"Small Cup", 120}, {"Teacup", 180}, {"Mug", 240}, {"Carafe", 500},
};
static const int hot_water_size_count = sizeof(hot_water_sizes) / sizeof(hot_water_sizes[0]);

// the preset closest to the current value, so values set elsewhere (web
// form, machine) still land on a sensible preset
inline int hotWaterNearest(const HotWaterPreset* presets, int count, int value) {
  int best = 0;
  for (int i = 1; i < count; i++) {
    if (abs(presets[i].value - value) < abs(presets[best].value - value)) {
      best = i;
    }
  }
  return best;
}

// the full width toggle rows on the hot water page, shared with hit testing
inline void adjustHotWaterRect(int width, int row, int& x, int& y, int& w, int& h) {
  x = px(10);
  y = px(48 + row * 40);
  w = width - px(20);
  h = px(32);
}

static const AdjustValue machine_adjust_values[] = {
    {"Sleep", "m", 5, 5, max_sleep_time, [](Config& c) { return c.getSleepTime(); },
     [](Config& c, int v) { c.setSleepTime(v); }},
    {"Flush", "s", 1, min_flush_seconds, max_flush_seconds, [](Config& c) { return c.getFlushSeconds(); },
     [](Config& c, int v) { c.setFlushSeconds(v); }},
};

static const char* orientation_names[] = {"Normal", "Flipped"};

static const char* units_names[] = {"Metric", "Imperial"};

static const AdjustValue app_adjust_values[] = {
    {"Orientation", "", 1, orientation_normal, orientation_flipped, [](Config& c) { return c.getOrientation(); },
     [](Config& c, int v) { c.setOrientation(v); }, orientation_names},
    {"Units", "", 1, units_metric, units_imperial, [](Config& c) { return c.getUnits(); },
     [](Config& c, int v) { c.setUnits(v); }, units_names},
};

// a coffee brown, a grind purple, a steel gray, a deep teal and a burgundy
// for these pages
const uint32_t espresso_page_color = 0x6A66;
const uint32_t grind_page_color = 0x7A77;
const uint32_t machine_page_color = 0x4A69;
const uint32_t app_page_color = 0x032C;
const uint32_t profile_page_color = 0x8926;
const uint32_t steam_page_color = 0x63D1;
const uint32_t hot_water_page_color = 0x1B6D;

static const AdjustPage adjust_pages[] = {
    {"Hot Water", TFT_WHITE, hot_water_page_color, hot_water_page_color, TFT_WHITE, TFT_WHITE, drawDrop, nullptr, 0,
     nullptr, false, AdjustPageKind::hot_water},
    {"Profile", TFT_WHITE, profile_page_color, profile_page_color, TFT_WHITE, TFT_WHITE, drawMug, nullptr, 0, nullptr,
     false, AdjustPageKind::profile},
    {"Water Levels", TFT_WHITE, theme.water_color, theme.water_color, TFT_WHITE, TFT_WHITE, drawDrop,
     water_adjust_values, 2},
    {"Espresso", TFT_WHITE, espresso_page_color, espresso_page_color, TFT_WHITE, TFT_WHITE, drawMug,
     espresso_adjust_values, 2},
    {"Grind Alerts", TFT_WHITE, grind_page_color, grind_page_color, TFT_WHITE, TFT_WHITE, drawGear,
     grind_adjust_values, 2},
    {"Steam", TFT_WHITE, steam_page_color, steam_page_color, TFT_WHITE, TFT_WHITE, drawPitcher, steam_adjust_values,
     2},
    {"Machine", TFT_WHITE, machine_page_color, machine_page_color, TFT_WHITE, TFT_WHITE, drawPower,
     machine_adjust_values, 2},
    {"App", TFT_WHITE, app_page_color, app_page_color, TFT_WHITE, TFT_WHITE, drawSliders, app_adjust_values, 2},
};
static const int adjust_page_count = sizeof(adjust_pages) / sizeof(adjust_pages[0]);

// the hot water page sits first so it's one swipe away and can be pulled up
// when the machine starts a hot water pour
const int adjust_hot_water_page = 0;

// button geometry in baseline design units, shared with the touch hit testing
const int adjust_button_r = 16;

// the header band height in design units
const int adjust_header_h = 40;

// button centers are anchored to the actual panel width: right aligned on
// the standard pages, centered under the label on centered pages
inline void adjustButtonCenter(const AdjustPage& page, int width, int row, bool plus, int& cx, int& cy) {
  if (page.centered) {
    cx = width / 2 + (plus ? px(47) : -px(47));
    cy = px(95 + row * 40);
  } else {
    cx = width - px(plus ? 26 : 121);
    cy = px(70 + row * 40);
  }
}

// the prev / next button centers on the profile page, centered on the panel
inline void adjustProfileButtonCenter(int width, bool next, int& cx, int& cy) {
  cx = width / 2 + (next ? px(50) : -px(50));
  cy = px(95);
}

// the rect a toggle value's single wide button occupies, spanning from the
// minus button's left edge to the plus button's right edge
inline void adjustToggleRect(const AdjustPage& page, int width, int row, int& x, int& y, int& w, int& h) {
  int cxMinus, cxPlus, cy;
  adjustButtonCenter(page, width, row, false, cxMinus, cy);
  adjustButtonCenter(page, width, row, true, cxPlus, cy);
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

    if (page.kind == AdjustPageKind::profile) {
      paintProfile(tft, page);
      tft.setTextDatum(TL_DATUM);
      return;
    }

    if (page.kind == AdjustPageKind::hot_water) {
      paintHotWater(tft, page);
      tft.setTextDatum(TL_DATUM);
      return;
    }

    for (int i = 0; i < page.valueCount; i++) {
      auto& value = page.values[i];
      int cxMinus, cxPlus, cy;
      adjustButtonCenter(page, m_width, i, false, cxMinus, cy);
      adjustButtonCenter(page, m_width, i, true, cxPlus, cy);

      tft.setFreeFont(FONT_BODY);
      if (page.centered) {
        // centered pages stack the label over the control
        tft.setTextDatum(MC_DATUM);
        tft.drawString(value.label, m_width / 2, px(62));
      } else {
        tft.setTextDatum(CL_DATUM);
        tft.drawString(value.label, px(10), cy);
      }

      if (value.names) {
        // named values render as one wide toggle button
        int bx, by, bw, bh;
        adjustToggleRect(page, m_width, i, bx, by, bw, bh);
        tft.fillRoundRect(bx, by, bw, bh, px(8), page.buttonColor);
        tft.setTextColor(page.bgColor, page.buttonColor);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(value.names[value.get(m_config) - value.min], bx + bw / 2, by + bh / 2);
        tft.setTextColor(page.textColor, page.bgColor);
        continue;
      }

      drawButton(tft, cxMinus, cy, false, page);

      char buffer[16];
      if (value.temp) {
        auto imperial = m_config.isImperial();
        snprintf(buffer, 16, "%d%s", displayTemp(value.get(m_config), imperial), tempUnit(imperial));
      } else {
        snprintf(buffer, 16, "%d%s", value.get(m_config), value.unit);
      }
      tft.setFreeFont(FONT_BODY);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(buffer, (cxMinus + cxPlus) / 2, cy);

      drawButton(tft, cxPlus, cy, true, page);
    }

    if (page.help) {
      tft.setFreeFont(FONT_BODY_SM);
      tft.setTextDatum(TC_DATUM);
      tft.drawString(page.help, m_width / 2, m_height - px(22));
    }

    tft.setTextDatum(TL_DATUM);
  }

 private:
  // the hot water page: two full width toggles, tea temperature presets and
  // cup sizes, showing whatever the config currently holds
  void paintHotWater(TFT_eSPI& tft, const AdjustPage& page) {
    auto imperial = m_config.isImperial();
    auto& tea = hot_water_teas[hotWaterNearest(hot_water_teas, hot_water_tea_count, m_config.getWaterTemp())];
    auto& size = hot_water_sizes[hotWaterNearest(hot_water_sizes, hot_water_size_count, m_config.getWaterVol())];

    char teaLabel[48];
    snprintf(teaLabel, 48, "%s (%d%s)", tea.name, displayTemp(tea.value, imperial), tempUnit(imperial));
    char sizeLabel[32];
    snprintf(sizeLabel, 32, "%s (%d%s)", size.name, displayVol(size.value, imperial), volUnit(imperial));
    const char* labels[2] = {teaLabel, sizeLabel};

    // oversized pours run the hot water button more than once
    if (size.value > hot_water_pour_max) {
      char note[40];
      auto pours = (size.value + hot_water_pour_max - 1) / hot_water_pour_max;
      snprintf(note, 40, "pours as %d x %d%s", pours, displayVol(size.value / pours, imperial), volUnit(imperial));
      tft.setFreeFont(FONT_BODY_SM);
      tft.setTextDatum(TC_DATUM);
      tft.drawString(note, m_width / 2, m_height - px(18));
    }

    for (int row = 0; row < 2; row++) {
      int bx, by, bw, bh;
      adjustHotWaterRect(m_width, row, bx, by, bw, bh);
      tft.fillRoundRect(bx, by, bw, bh, px(8), page.buttonColor);
      tft.setTextColor(page.bgColor, page.buttonColor);
      tft.setFreeFont(FONT_BODY);
      if (tft.textWidth(labels[row]) > bw - px(8)) {
        tft.setFreeFont(FONT_BODY_SM);
      }
      tft.setTextDatum(MC_DATUM);
      tft.drawString(labels[row], bx + bw / 2, by + bh / 2);
    }
    tft.setTextColor(page.textColor, page.bgColor);
  }

  // the profile page: the selected profile's name with prev / next buttons
  // and its position within the enabled set
  void paintProfile(TFT_eSPI& tft, const AdjustPage& page) {
    auto profile = m_config.getProfile();
    if (profile >= 1 && profile <= profile_count) {
      // long names step down a font size, either way they stay centered
      auto name = profiles[profile - 1].name;
      tft.setFreeFont(FONT_BODY);
      if (tft.textWidth(name) > m_width - px(12)) {
        tft.setFreeFont(FONT_BODY_SM);
      }
      tft.setTextDatum(MC_DATUM);
      tft.drawString(name, m_width / 2, px(62));
    }

    // where we sit within the enabled profiles
    int pos = 0;
    for (int i = 0; i < profile; i++) {
      pos += m_config.isProfileEnabled(i);
    }
    char buffer[16];
    snprintf(buffer, 16, "%d/%d", pos, m_config.enabledProfileCount());
    tft.setFreeFont(FONT_BODY);
    tft.setTextDatum(MC_DATUM);
    int cxPrev, cxNext, cy;
    adjustProfileButtonCenter(m_width, false, cxPrev, cy);
    adjustProfileButtonCenter(m_width, true, cxNext, cy);
    tft.drawString(buffer, (cxPrev + cxNext) / 2, cy);

    drawArrowButton(tft, cxPrev, cy, false, page);
    drawArrowButton(tft, cxNext, cy, true, page);
  }

  void drawArrowButton(TFT_eSPI& tft, int cx, int cy, bool next, const AdjustPage& page) {
    auto r = px(adjust_button_r);
    tft.fillCircle(cx, cy, r, page.buttonColor);
    auto d = next ? 1 : -1;
    tft.fillTriangle(cx + d * r / 2, cy, cx - d * r / 3, cy - r / 2, cx - d * r / 3, cy + r / 2, page.bgColor);
  }

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
