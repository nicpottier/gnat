#pragma once

// our widgets are laid out against the original 240x135 panel, denser panels
// scale dimensions up by UI_SCALE_PCT (percent) to keep the same physical size
#ifndef UI_SCALE_PCT
#define UI_SCALE_PCT 100
#endif

// scales a dimension from our baseline design to the actual panel
inline int px(int v) {
  return v * UI_SCALE_PCT / 100;
}

// font tiers, scaled panels step everything up to match physically
#if UI_SCALE_PCT >= 150
#define FONT_BODY &FreeSans18pt7b
#define FONT_BODY_SM &FreeSans12pt7b
#define FONT_SUB &FreeSansBold18pt7b
#define FONT_TITLE &FreeSansBold24pt7b
#else
#define FONT_BODY &FreeSans9pt7b
#define FONT_BODY_SM &FreeSans9pt7b
#define FONT_SUB &FreeSansBold12pt7b
#define FONT_TITLE &FreeSansBold18pt7b
#endif

// touch dismiss button placement (from the bottom right corner) and size,
// shared between the banner painting and the touch hit test
#define DISMISS_BTN_MARGIN 30
#define DISMISS_BTN_R 20
