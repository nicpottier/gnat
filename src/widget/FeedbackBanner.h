#pragma once

#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {

// full screen banner shown after a shot that missed the target time or when
// the tank needs water, the profile button dismisses it
class FeedbackBanner : public Widget {
 public:
  FeedbackBanner(int width, int height)
      : m_width{width},
        m_height{height} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) {
    // when previewing, show a sample water level
    auto waterLevel = ctx.feedbackPreview ? 7 : ctx.waterLevel;

    if (ctx.feedback != m_feedback ||
        (m_feedback == FeedbackType::add_water && waterLevel != m_waterLevel) ||
        ctx.config.finerIsLeft() != m_finerLeft || ctx.config.isFlipped() != m_flipped) {
      m_feedback = ctx.feedback;
      m_feedbackSeconds = ctx.feedbackSeconds;
      m_waterLevel = waterLevel;
      m_finerLeft = ctx.config.finerIsLeft();
      m_flipped = ctx.config.isFlipped();
      return true;
    }

    return false;
  }

  void paint(TFT_eSPI& tft) {
    if (m_feedback == FeedbackType::none) {
      return;
    }

    const char* msg;
    char buffer[16];
    uint32_t bg;
    uint32_t fg;

    if (m_feedback == FeedbackType::add_water) {
      msg = "Add Water";
      snprintf(buffer, 16, "%dmm left", m_waterLevel);
      bg = theme.water_color;
      fg = TFT_WHITE;
    } else if (m_feedback == FeedbackType::nailed_it) {
      msg = "Nailed it!";
      snprintf(buffer, 16, "%0.1fs shot", m_feedbackSeconds);
      bg = theme.banner_good_color;
      fg = theme.bg_color;
    } else {
      auto finer = m_feedback == FeedbackType::grind_finer;
      msg = finer ? "Grind Finer" : "Grind Coarser";
      snprintf(buffer, 16, "%ds shot", int(round(m_feedbackSeconds)));
      bg = finer ? theme.banner_color : theme.banner_alt_color;
      fg = theme.bg_color;
    }

    tft.fillScreen(bg);

    // icon centered between the top of the display and the title text
    auto titleTop = m_height / 2 + 6 - 13;
    auto iconCy = titleTop / 2;

    if (m_feedback == FeedbackType::add_water) {
      drawDrop(tft, m_width / 2, iconCy, fg, bg);
    } else if (m_feedback == FeedbackType::nailed_it) {
      drawCheck(tft, m_width / 2, iconCy, fg);
    } else {
      // finer and coarser rotate opposite ways
      auto left = (m_feedback == FeedbackType::grind_finer) ? m_finerLeft : !m_finerLeft;
      drawRotation(tft, m_width / 2, iconCy, 17, !left, fg);
    }

    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold18pt7b);
    tft.setTextColor(fg, bg);
    tft.drawString(msg, m_width / 2, m_height / 2 + 6);

    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.drawString(buffer, m_width / 2, m_height / 2 + 38);
    tft.setTextDatum(TL_DATUM);

    // checkmark hint by the physical dismiss button, bottom left normally,
    // bottom right when the device is flipped
    if (m_flipped) {
      drawDismiss(tft, m_width - 20, m_height - 20, fg, bg);
    } else {
      drawDismiss(tft, 20, m_height - 20, fg, bg);
    }
  }

 private:
  // draws a circular arrow, a 270 degree arc with an arrowhead at the end
  void drawRotation(TFT_eSPI& tft, int cx, int cy, int r, bool clockwise, uint32_t color) {
    const double start = -M_PI * 0.25;
    const double span = M_PI * 1.5;
    const int segments = 45;

    // stroke the arc a few radii thick
    for (int ring = -2; ring <= 2; ring++) {
      int prevX = 0;
      int prevY = 0;
      for (int i = 0; i <= segments; i++) {
        double a = start + span * i / segments;
        int x = cx + round(cos(a) * (r + ring) * (clockwise ? -1 : 1));
        int y = cy - round(sin(a) * (r + ring));
        if (i > 0) {
          tft.drawLine(prevX, prevY, x, y, color);
        }
        prevX = x;
        prevY = y;
      }
    }

    // arrowhead at the end of the arc, pointing along the direction of travel
    double a = start + span;
    double dir = clockwise ? -1 : 1;
    double px = cx + cos(a) * r * dir;
    double py = cy - sin(a) * r;

    // tangent along travel and the radial normal
    double tx = -sin(a) * dir;
    double ty = -cos(a);
    double nx = cos(a) * dir;
    double ny = -sin(a);

    tft.fillTriangle(round(px + tx * 11), round(py + ty * 11), round(px + nx * 6), round(py + ny * 6),
                     round(px - nx * 6), round(py - ny * 6), color);
  }

  // draws an outlined water drop with roughly the same size and stroke weight
  // as the rotation icon
  void drawDrop(TFT_eSPI& tft, int cx, int cy, uint32_t color, uint32_t bg) {
    fillDrop(tft, cx, cy - 17, cy + 6, 10, color);
    fillDrop(tft, cx, cy - 10, cy + 6, 6, bg);
  }

  // a big checkmark matching the size and stroke weight of our other icons
  void drawCheck(TFT_eSPI& tft, int cx, int cy, uint32_t color) {
    for (int i = -2; i <= 2; i++) {
      tft.drawLine(cx - 14, cy + i, cx - 5, cy + 9 + i, color);
      tft.drawLine(cx - 5, cy + 9 + i, cx + 14, cy - 10 + i, color);
    }
  }

  // a small checkmark button hinting that a button press dismisses us
  void drawDismiss(TFT_eSPI& tft, int cx, int cy, uint32_t color, uint32_t bg) {
    tft.fillCircle(cx, cy, 11, color);
    for (int i = 0; i < 3; i++) {
      tft.drawLine(cx - 5, cy + i - 1, cx - 1, cy + 3 + i, bg);
      tft.drawLine(cx - 1, cy + 3 + i, cx + 5, cy - 4 + i, bg);
    }
  }

  // a filled teardrop, a triangle from the apex over a round bottom
  void fillDrop(TFT_eSPI& tft, int cx, int apexY, int circleY, int r, uint32_t color) {
    auto half = round(r * 0.72);
    auto baseY = circleY - round(r * 0.3);
    tft.fillTriangle(cx, apexY, cx - half, baseY, cx + half, baseY, color);
    tft.fillCircle(cx, circleY, r, color);
  }

  int m_width;
  int m_height;

  FeedbackType m_feedback = FeedbackType::none;
  double m_feedbackSeconds = 0;
  int m_waterLevel = 0;
  bool m_finerLeft = true;
  bool m_flipped = false;
};

}  // namespace widget
