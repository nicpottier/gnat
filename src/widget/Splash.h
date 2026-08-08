#pragma once

#include <math.h>

#include <widget/Logo.h>
#include <widget/Theme.h>
#include <widget/Widget.h>

// TODO: this is a dupe of main content, move to separate include
#define STRINGIZER(arg) #arg
#define STR_VALUE(arg) STRINGIZER(arg)
#define SPLASH_VERSION STR_VALUE(BUILD_VERSION)

namespace widget {

// the splash animates like a pulled shot: a stream pours from the top of the
// panel into the T, the liquid rises and sloshes, settles flat, then a glint
// sweeps across the filled logo
const unsigned long splash_delay_ms = 1000;
const unsigned long splash_fill_ms = 3600;
const unsigned long splash_settle_ms = 400;
const unsigned long splash_glint_ms = 500;

// unfilled logo outline, the crema layer, the darker espresso body under it
// and the brighter liquid surface
const uint16_t splash_dim_color = 0x31A6;
const uint16_t splash_crema_color = 0xD4A9;
const uint16_t splash_body_color = 0x69C2;
const uint16_t splash_surface_color = 0xF5CC;

// how deep the crema layer sits, as a fraction of the liquid depth
const float splash_crema_depth = 0.20f;

// where the pour lands: the center of the T's stem, in source columns
const int splash_pour_x = 140;

// half width of the glint band, in diagonal source pixels
const float splash_glint_band = 14;

// how fast the stream's ends fall, in source pixels per milli
const float splash_stream_speed = 0.45f;

class Splash : public Widget {
 public:
  Splash(int width, int height)
      : m_width{width},
        m_height{height} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) {
    // we only tick while visible, so a gap since our last tick means we were
    // just shown again, restart the animation
    if (millis - m_lastTick > 250) {
      m_start = millis;
      m_bgPainted = false;
    }
    m_lastTick = millis;

    // keep repainting until the glint has passed
    return millis - m_start <= splash_delay_ms + splash_fill_ms + splash_settle_ms + splash_glint_ms + 50;
  }

  void paint(TFT_eSPI& tft) {
    const int scale = (UI_SCALE_PCT + 50) / 100;
    const int w = splash_sprite_width * scale;
    const int h = splash_sprite_height * scale;
    const int x = (m_width - w) / 2;
    const int y = (m_height - h) / 2;

    // how many source rows of pour stream sit between the panel top and the
    // logo, we repaint those along with the logo itself
    const int pourRows = y / scale;

    // the background and version only need painting when first shown
    if (!m_bgPainted) {
      m_bgPainted = true;
      tft.fillRect(0, 0, m_width, m_height, theme.bg_color);
      tft.setFreeFont(FONT_BODY);
      tft.setTextColor(theme.text_color, theme.bg_color);
      tft.setTextDatum(BC_DATUM);
      tft.drawString(SPLASH_VERSION, m_width / 2, m_height - px(8));
      tft.setTextDatum(TL_DATUM);
    }

    auto t = m_lastTick - m_start;
    float tsec = t / 1000.0f;

    // time since the pour started, negative while we hold on the dim logo
    long tp = long(t) - long(splash_delay_ms);

    const float H = splash_sprite_height;
    const float W = splash_sprite_width;

    // how far the liquid has risen, 0.0 to 1.0
    float fillP = tp >= long(splash_fill_ms) ? 1.0f : tp <= 0 ? 0.0f : float(tp) / splash_fill_ms;

    // the waves ramp in as the liquid arrives and die out while it settles
    float settleP = tp <= long(splash_fill_ms) ? 0.0f : fminf(1.0f, float(tp - long(splash_fill_ms)) / splash_settle_ms);
    float env = (1.0f - settleP) * fminf(1.0f, fmaxf(0.0f, float(tp)) / 250.0f);

    // the liquid surface per column: a rising base, a side to side sloshing
    // tilt, two traveling ripples and a mound where the stream lands
    float base = (H + 3) * (1.0f - fillP);
    float surf[splash_sprite_width];
    for (int sx = 0; sx < (int)W; sx++) {
      float tilt = 6.5f * sinf(tsec * 5.5f) * ((sx - W / 2) / W);
      float ripple = 1.3f * sinf(sx * 0.20f - tsec * 10.0f) + 0.9f * sinf(sx * 0.11f + tsec * 6.3f);
      float dx = (sx - splash_pour_x) / 7.0f;
      float mound = -2.5f * expf(-dx * dx);
      surf[sx] = base + (tilt + ripple + mound) * env;
    }

    // the stream's leading edge falls once the pour starts, its tail falls
    // once the pour stops
    bool pouring = tp < long(splash_fill_ms);
    float streamTip = -float(pourRows) + tp * splash_stream_speed;
    float streamTail = pouring ? -float(pourRows) : -float(pourRows) + (tp - long(splash_fill_ms)) * splash_stream_speed;

    // where the glint band sits in diagonal source units, parked well off
    // the logo before and after its sweep
    float glint = -1000;
    long glintT = tp - long(splash_fill_ms + splash_settle_ms);
    if (glintT >= 0 && glintT <= long(splash_glint_ms)) {
      float span = W + H / 2 + 2 * splash_glint_band;
      glint = float(glintT) / splash_glint_ms * span - splash_glint_band;
    }

    uint16_t row[splash_sprite_width * 4];
    for (int sy = -pourRows; sy < (int)H; sy++) {
      for (int sx = 0; sx < (int)W; sx++) {
        uint16_t c = theme.bg_color;

        bool lit = sy >= 0 && splash_sprite[sy * (int)W + sx] != 0;
        if (lit) {
          float edge = sy - surf[sx];

          // the crema floats on top of the darker body, its layer growing
          // with the liquid so it's always the top tenth of the pour
          float cremaBottom = fmaxf(3.0f, (H - surf[sx]) * splash_crema_depth);

          if (edge < 0) {
            c = splash_dim_color;
          } else if (edge < 2) {
            // the bright surface fades into the crema as the liquid settles
            c = lerp565(splash_surface_color, splash_crema_color, settleP);
          } else if (edge < cremaBottom) {
            c = splash_crema_color;
          } else {
            c = splash_body_color;
          }

          // brighten toward white as the glint band passes
          float d = fabsf(sx + sy / 2.0f - glint);
          if (d < splash_glint_band) {
            c = lerp565(c, 0xFFFF, 1.0f - d / splash_glint_band);
          }
        }

        // the stream draws over everything on its way into the liquid
        if (abs(sx - splash_pour_x) <= 1 && sy >= streamTail && sy < streamTip && sy < surf[sx]) {
          c = splash_surface_color;
        }

        for (int i = 0; i < scale; i++) {
          row[sx * scale + i] = c;
        }
      }
      for (int i = 0; i < scale; i++) {
        pushLogoRow(tft, x, y + sy * scale + i, w, row);
      }
    }
  }

 private:
  int m_width;
  int m_height;

  // when the current showing started and when we last ticked
  unsigned long m_start = 0;
  unsigned long m_lastTick = 0;

  // whether the background and version have been painted this showing
  bool m_bgPainted = false;
};

}  // namespace widget
