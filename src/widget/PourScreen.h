#pragma once

#include <math.h>

#include <profiles.h>
#include <widget/Splash.h>
#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {

// the pour screen: a big shot timer with live pressure and flow, and a cup
// that fills with the shot the same way the boot splash pours, its level
// tracking weight against the configured stop weight

// cup dimensions and position in baseline design units
const int pour_cup_w = 64;
const int pour_cup_h = 58;

// the cup visualizes this many grams when full, so a typical shot fills
// about a third of it
const int pour_cup_capacity_g = 100;

class PourScreen : public Widget {
 public:
  PourScreen(int width, int height)
      : m_width{width},
        m_height{height} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) {
    bool changed = false;

    // track pour start / end for the timer and the slosh envelope
    bool pouring = ctx.machineState == MachineState::espresso && ctx.machineSubstate == MachineSubstate::pouring;
    if (pouring && !m_pouring) {
      m_pourStart = millis;
      m_shotTime = 0;
    }
    if (!pouring && m_pouring) {
      m_pourEnd = millis;
      m_shotTime = millis - m_pourStart;
    }
    m_pouring = pouring;
    m_now = millis;

    auto weight = int(ctx.currentWeight * 10) / 10.0;
    auto pressure = int(ctx.lastSample.groupPressure * 10) / 10.0;
    auto flow = int(ctx.lastSample.groupFlow * 10) / 10.0;
    auto scale = ctx.getScaleBLEState();

    if (weight != m_weight || pressure != m_pressure || flow != m_flow || scale != m_scaleState ||
        ctx.config.getProfile() != m_profile) {
      m_weight = weight;
      m_pressure = pressure;
      m_flow = flow;
      m_scaleState = scale;
      m_profile = ctx.config.getProfile();
      changed = true;
    }

    // animate the slosh while pouring and while it settles, tick the timer
    // along while any shot state is active
    if (m_pouring || m_now - m_pourEnd < 700) {
      changed = true;
    } else if (ctx.machineState == MachineState::espresso && tickID % 5 == 0) {
      changed = true;
    }

    return changed;
  }

  void paint(TFT_eSPI& tft) {
    tft.fillRect(0, 0, m_width, m_height, theme.bg_color);

    // cup zone on the right, the rest is the readout zone
    auto cupW = px(pour_cup_w);
    auto cupH = px(pour_cup_h);
    auto cupX = m_width - cupW - px(20);
    auto cupY = (m_height - cupH) / 2 - px(6);
    auto leftW = cupX - px(16);

    paintCup(tft, cupX, cupY, cupW, cupH);

    // the big shot timer
    char buffer[16];
    auto elapsed = m_shotTime > 0 ? m_shotTime : (m_pouring ? m_now - m_pourStart : 0);
    snprintf(buffer, 16, "%0.1fs", elapsed / 1000.0);
    tft.setFreeFont(FONT_TITLE);
    tft.setTextSize(2);
    tft.setTextColor(theme.text_color, theme.bg_color);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(buffer, leftW / 2, m_height / 2 - px(12));
    tft.setTextSize(1);

    // live pressure and flow in their trace colors
    tft.setFreeFont(FONT_BODY);
    snprintf(buffer, 16, "%0.1f bar", m_pressure);
    tft.setTextColor(theme.pressure_color, theme.bg_color);
    tft.drawString(buffer, leftW / 4 + px(8), m_height / 2 + px(28));

    snprintf(buffer, 16, "%0.1f ml/s", m_flow);
    tft.setTextColor(theme.water_color, theme.bg_color);
    tft.drawString(buffer, (leftW * 3) / 4 - px(8), m_height / 2 + px(28));

    // the current profile along the bottom
    if (m_profile >= 1 && m_profile <= profile_count) {
      tft.setFreeFont(FONT_BODY_SM);
      tft.setTextColor(theme.text_color, theme.bg_color);
      tft.setTextDatum(BL_DATUM);
      tft.drawString(profiles[m_profile - 1].name, px(8), m_height - px(6));
    }

    tft.setTextDatum(TL_DATUM);
  }

 private:
  void paintCup(TFT_eSPI& tft, int cupX, int cupY, int cupW, int cupH) {
    // the handle pokes out the right side, the body draws over its left half
    tft.drawCircle(cupX + cupW + px(4), cupY + cupH / 2, px(12), theme.text_color);
    tft.drawCircle(cupX + cupW + px(4), cupY + cupH / 2, px(8), theme.text_color);
    tft.fillRect(cupX, cupY, cupW, cupH, theme.bg_color);

    // interior of the cup
    auto ix = cupX + px(3);
    auto iy = cupY + px(3);
    auto iw = cupW - px(6);
    auto ih = cupH - px(6);

    // how full we are against the cup's capacity
    float fillP = 0;
    if (m_scaleState == BLEState::connected && m_weight > 0) {
      fillP = fminf(1.0f, float(m_weight) / pour_cup_capacity_g);
    }

    // the slosh ramps in as the pour starts and dies off after it stops
    float env = 0;
    if (m_pouring) {
      env = fminf(1.0f, (m_now - m_pourStart) / 250.0f);
    } else if (m_pourEnd > 0) {
      env = fmaxf(0.0f, 1.0f - (m_now - m_pourEnd) / 600.0f);
    }

    float base = iy + ih * (1.0f - fillP);
    float tsec = m_now / 1000.0f;
    int xc = ix + iw / 2;

    // liquid drawn column by column so the surface can slosh, the crema
    // riding as the top fifth over the darker body
    for (int col = 0; col < iw; col++) {
      float tilt = px(2) * sinf(tsec * 5.5f) * ((col - iw / 2.0f) / iw);
      float ripple = 0.4f * px(1) * sinf(col * 24.0f / iw - tsec * 9.0f);
      float dxs = (col + ix - xc) / float(px(5));
      float mound = -px(1) * expf(-dxs * dxs);
      int surf = int(base + (tilt + ripple + mound) * env);

      if (surf < iy) {
        surf = iy;
      }
      auto bottom = iy + ih;
      if (surf >= bottom) {
        continue;
      }

      auto cremaBottom = surf + max(px(2), int((bottom - surf) * splash_crema_depth));
      tft.drawFastVLine(ix + col, surf, min(px(2), bottom - surf), splash_surface_color);
      if (surf + px(2) < cremaBottom) {
        tft.drawFastVLine(ix + col, surf + px(2), cremaBottom - surf - px(2), splash_crema_color);
      }
      if (cremaBottom < bottom) {
        tft.drawFastVLine(ix + col, cremaBottom, bottom - cremaBottom, splash_body_color);
      }
    }

    // cup walls over the liquid so slosh stays inside
    tft.drawRoundRect(cupX, cupY, cupW, cupH, px(6), theme.text_color);
    tft.drawRoundRect(cupX + 1, cupY + 1, cupW - 2, cupH - 2, px(6), theme.text_color);

    // the stream falls from the top of the panel into the cup while pouring
    if (m_pouring) {
      auto surfY = int(base) - px(1);
      if (surfY > 0) {
        tft.fillRect(xc - px(1), 0, px(2), surfY, splash_surface_color);
      }
    }

    // the weight rides under the cup
    char buffer[10];
    if (m_scaleState == BLEState::connected) {
      snprintf(buffer, 10, "%0.1fg", m_weight);
    } else {
      snprintf(buffer, 10, "--g");
    }
    tft.setFreeFont(FONT_BODY);
    tft.setTextColor(splash_crema_color, theme.bg_color);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(buffer, cupX + cupW / 2, cupY + cupH + px(12));
  }

  int m_width;
  int m_height;

  bool m_pouring = false;
  unsigned long m_pourStart = 0;
  unsigned long m_pourEnd = 0;
  unsigned long m_shotTime = 0;
  unsigned long m_now = 0;

  double m_weight = 0;
  double m_pressure = 0;
  double m_flow = 0;
  int m_profile = 0;
  BLEState m_scaleState = BLEState::unknown;
};

}  // namespace widget
