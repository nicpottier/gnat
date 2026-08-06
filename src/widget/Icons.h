#pragma once

#include <widget/Layout.h>
#include <widget/Theme.h>

#ifdef M5_STICK
#include <M5Display.h>
#else
#include <TFT_eSPI.h>
#endif

namespace widget {

// shared icon painters, sized in baseline design units around a center point

// a circular arrow, a 270 degree arc with an arrowhead at the end
inline void drawRotation(TFT_eSPI& tft, int cx, int cy, int r, bool clockwise, uint32_t color) {
  const double start = -M_PI * 0.25;
  const double span = M_PI * 1.5;
  const int segments = 45;

  // stroke the arc a few radii thick
  int stroke = UI_SCALE_PCT >= 150 ? 4 : 2;
  for (int ring = -stroke; ring <= stroke; ring++) {
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
  double ax = cx + cos(a) * r * dir;
  double ay = cy - sin(a) * r;

  // tangent along travel and the radial normal
  double tx = -sin(a) * dir;
  double ty = -cos(a);
  double nx = cos(a) * dir;
  double ny = -sin(a);

  auto al = px(11);
  auto aw = px(6);
  tft.fillTriangle(round(ax + tx * al), round(ay + ty * al), round(ax + nx * aw), round(ay + ny * aw),
                   round(ax - nx * aw), round(ay - ny * aw), color);
}

// a filled teardrop, a triangle from the apex over a round bottom
inline void fillDrop(TFT_eSPI& tft, int cx, int apexY, int circleY, int r, uint32_t color) {
  auto half = round(r * 0.72);
  auto baseY = circleY - round(r * 0.3);
  tft.fillTriangle(cx, apexY, cx - half, baseY, cx + half, baseY, color);
  tft.fillCircle(cx, circleY, r, color);
}

// an outlined water drop
inline void drawDrop(TFT_eSPI& tft, int cx, int cy, uint32_t color, uint32_t bg) {
  fillDrop(tft, cx, cy - px(17), cy + px(6), px(10), color);
  fillDrop(tft, cx, cy - px(10), cy + px(6), px(6), bg);
}

// a big checkmark matching the size and stroke weight of the other icons
inline void drawCheck(TFT_eSPI& tft, int cx, int cy, uint32_t color) {
  int stroke = UI_SCALE_PCT >= 150 ? 4 : 2;
  for (int i = -stroke; i <= stroke; i++) {
    tft.drawLine(cx - px(14), cy + i, cx - px(5), cy + px(9) + i, color);
    tft.drawLine(cx - px(5), cy + px(9) + i, cx + px(14), cy - px(10) + i, color);
  }
}

// a checkmark button hinting how banners get dismissed, check scales with radius
inline void drawDismiss(TFT_eSPI& tft, int cx, int cy, int r, uint32_t color, uint32_t bg) {
  tft.fillCircle(cx, cy, r, color);
  int stroke = max(3, r / 4);
  for (int i = 0; i < stroke; i++) {
    tft.drawLine(cx - r / 2, cy + i - r / 8, cx - r / 8, cy + r / 3 + i, bg);
    tft.drawLine(cx - r / 8, cy + r / 3 + i, cx + r / 2, cy - r / 3 + i, bg);
  }
}

// a simple alarm clock, a face with hands and bell stubs
inline void drawClock(TFT_eSPI& tft, int cx, int cy, uint32_t color, uint32_t bg) {
  auto r = px(13);
  int stroke = UI_SCALE_PCT >= 150 ? 3 : 2;

  // face a few rings thick
  for (int i = 0; i < stroke; i++) {
    tft.drawCircle(cx, cy, r - i, color);
  }

  // hands pointing at twelve and three
  for (int i = 0; i < stroke; i++) {
    tft.drawLine(cx + i - stroke / 2, cy, cx + i - stroke / 2, cy - r + px(4), color);
    tft.drawLine(cx, cy + i - stroke / 2, cx + r - px(4), cy + i - stroke / 2, color);
  }

  // bell stubs on top
  for (int i = 0; i < stroke; i++) {
    tft.drawLine(cx - r / 2 + i, cy - r + px(2), cx - r + i, cy - r - px(4), color);
    tft.drawLine(cx + r / 2 + i, cy - r + px(2), cx + r + i, cy - r - px(4), color);
  }
}

// an outlined mug with a handle
inline void drawMug(TFT_eSPI& tft, int cx, int cy, uint32_t color, uint32_t bg) {
  auto w = px(20);
  auto h = px(18);
  int stroke = UI_SCALE_PCT >= 150 ? 4 : 2;

  // handle ring off the right side
  for (int i = 0; i < stroke; i++) {
    tft.drawCircle(cx + w / 2 - px(2), cy - px(1), px(7) - i, color);
  }

  // body over the handle
  tft.fillRoundRect(cx - w / 2 - px(4), cy - h / 2, w, h, px(3), color);
  tft.fillRoundRect(cx - w / 2 - px(4) + stroke, cy - h / 2 + stroke, w - stroke * 2, h - stroke * 2, px(2), bg);
}

// a power symbol, a broken ring with a bar through the top
inline void drawPower(TFT_eSPI& tft, int cx, int cy, uint32_t color, uint32_t bg) {
  auto r = px(12);
  int stroke = UI_SCALE_PCT >= 150 ? 3 : 2;

  for (int i = 0; i < stroke; i++) {
    tft.drawCircle(cx, cy + px(2), r - i, color);
  }

  // gap in the ring where the bar sits
  tft.fillRect(cx - px(4), cy + px(2) - r - px(2), px(8), px(7), bg);

  // the bar
  tft.fillRect(cx - stroke / 2 - 1, cy - r - px(2), stroke + 1, px(11), color);
}

// a gear, a toothed ring with a center hole
inline void drawGear(TFT_eSPI& tft, int cx, int cy, uint32_t color, uint32_t bg) {
  auto r = px(11);

  // eight teeth around the body
  for (int t = 0; t < 8; t++) {
    double a = t * M_PI / 4;
    tft.fillCircle(cx + round(cos(a) * (r + px(2))), cy + round(sin(a) * (r + px(2))), px(3), color);
  }

  // body with a hole in the middle
  tft.fillCircle(cx, cy, r, color);
  tft.fillCircle(cx, cy, px(5), bg);
}

// three slider rows, a generic settings icon
inline void drawSliders(TFT_eSPI& tft, int cx, int cy, uint32_t color, uint32_t bg) {
  int stroke = UI_SCALE_PCT >= 150 ? 3 : 2;
  int knobs[] = {-px(6), px(5), -px(2)};

  for (int row = 0; row < 3; row++) {
    auto y = cy + (row - 1) * px(10);
    tft.fillRect(cx - px(14), y - stroke / 2, px(28), stroke, color);
    tft.fillCircle(cx + knobs[row], y, px(4), color);
  }
}

}  // namespace widget
