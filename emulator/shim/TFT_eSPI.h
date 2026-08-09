#pragma once

// a software canvas standing in for TFT_eSPI: every draw call the gnat
// widgets use renders into an rgb565 buffer, sprites included, with the
// adafruit gfx free fonts for text

#include <map>
#include <vector>

#include "Arduino.h"

typedef struct {
  uint32_t bitmapOffset;
  uint8_t width, height;
  uint8_t xAdvance;
  int8_t xOffset, yOffset;
} GFXglyph;

typedef struct {
  uint8_t* bitmap;
  GFXglyph* glyph;
  uint16_t first, last;
  uint8_t yAdvance;
} GFXfont;

#include "fonts/FreeSans12pt7b.h"
#include "fonts/FreeSans18pt7b.h"
#include "fonts/FreeSans9pt7b.h"
#include "fonts/FreeSansBold12pt7b.h"
#include "fonts/FreeSansBold18pt7b.h"
#include "fonts/FreeSansBold24pt7b.h"

#define TFT_WHITE 0xFFFF
#define TFT_BLACK 0x0000

#define TL_DATUM 0
#define TC_DATUM 1
#define TR_DATUM 2
#define ML_DATUM 3
#define CL_DATUM 3
#define MC_DATUM 4
#define CC_DATUM 4
#define MR_DATUM 5
#define CR_DATUM 5
#define BL_DATUM 6
#define BC_DATUM 7
#define BR_DATUM 8

#define ST7789_DISPOFF 0x28
#define ST7789_DISPON 0x29

class TFT_eSPI {
 public:
  TFT_eSPI(int w = emu_panel_w, int h = emu_panel_h) { resize(w, h); }
  virtual ~TFT_eSPI() {}

  void init() {}
  void setRotation(int) {}
  void setSwapBytes(bool) {}
  void writecommand(uint8_t) {}
  int16_t width() { return m_w; }
  int16_t height() { return m_h; }

  void drawPixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= m_w || y < 0 || y >= m_h) {
      return;
    }
    m_buf[y * m_w + x] = (uint16_t)color;
  }

  void drawFastHLine(int x, int y, int w, uint32_t color) {
    for (int i = 0; i < w; i++) {
      drawPixel(x + i, y, color);
    }
  }

  void drawFastVLine(int x, int y, int h, uint32_t color) {
    for (int i = 0; i < h; i++) {
      drawPixel(x, y + i, color);
    }
  }

  void drawLine(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
      drawPixel(x0, y0, color);
      if (x0 == x1 && y0 == y1) {
        break;
      }
      int e2 = 2 * err;
      if (e2 >= dy) {
        err += dy;
        x0 += sx;
      }
      if (e2 <= dx) {
        err += dx;
        y0 += sy;
      }
    }
  }

  void fillRect(int x, int y, int w, int h, uint32_t color) {
    for (int row = 0; row < h; row++) {
      drawFastHLine(x, y + row, w, color);
    }
  }

  void fillScreen(uint32_t color) { fillRect(0, 0, m_w, m_h, color); }

  void drawRect(int x, int y, int w, int h, uint32_t color) {
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, y + h - 1, w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine(x + w - 1, y, h, color);
  }

  void drawCircle(int cx, int cy, int r, uint32_t color) {
    int x = 0, y = r, d = 3 - 2 * r;
    while (y >= x) {
      drawPixel(cx + x, cy + y, color);
      drawPixel(cx - x, cy + y, color);
      drawPixel(cx + x, cy - y, color);
      drawPixel(cx - x, cy - y, color);
      drawPixel(cx + y, cy + x, color);
      drawPixel(cx - y, cy + x, color);
      drawPixel(cx + y, cy - x, color);
      drawPixel(cx - y, cy - x, color);
      x++;
      if (d > 0) {
        y--;
        d += 4 * (x - y) + 10;
      } else {
        d += 4 * x + 6;
      }
    }
  }

  void fillCircle(int cx, int cy, int r, uint32_t color) {
    for (int y = -r; y <= r; y++) {
      for (int x = -r; x <= r; x++) {
        if (x * x + y * y <= r * r) {
          drawPixel(cx + x, cy + y, color);
        }
      }
    }
  }

  void drawRoundRect(int x, int y, int w, int h, int r, uint32_t color) {
    r = min(r, min(w, h) / 2);
    drawFastHLine(x + r, y, w - 2 * r, color);
    drawFastHLine(x + r, y + h - 1, w - 2 * r, color);
    drawFastVLine(x, y + r, h - 2 * r, color);
    drawFastVLine(x + w - 1, y + r, h - 2 * r, color);
    circleQuadrant(x + r, y + r, r, 1, false, color);
    circleQuadrant(x + w - r - 1, y + r, r, 2, false, color);
    circleQuadrant(x + w - r - 1, y + h - r - 1, r, 4, false, color);
    circleQuadrant(x + r, y + h - r - 1, r, 8, false, color);
  }

  void fillRoundRect(int x, int y, int w, int h, int r, uint32_t color) {
    r = min(r, min(w, h) / 2);
    fillRect(x, y + r, w, h - 2 * r, color);
    fillRect(x + r, y, w - 2 * r, r, color);
    fillRect(x + r, y + h - r, w - 2 * r, r, color);
    circleQuadrant(x + r, y + r, r, 1, true, color);
    circleQuadrant(x + w - r - 1, y + r, r, 2, true, color);
    circleQuadrant(x + w - r - 1, y + h - r - 1, r, 4, true, color);
    circleQuadrant(x + r, y + h - r - 1, r, 8, true, color);
  }

  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color) {
    // sort by y then rasterize scanlines, the classic adafruit approach
    if (y0 > y1) {
      std::swap(y0, y1);
      std::swap(x0, x1);
    }
    if (y1 > y2) {
      std::swap(y2, y1);
      std::swap(x2, x1);
    }
    if (y0 > y1) {
      std::swap(y0, y1);
      std::swap(x0, x1);
    }
    if (y0 == y2) {
      int a = min(x0, min(x1, x2));
      int b = max(x0, max(x1, x2));
      drawFastHLine(a, y0, b - a + 1, color);
      return;
    }
    for (int y = y0; y <= y2; y++) {
      double xa = x0 + (double)(x2 - x0) * (y - y0) / (y2 - y0);
      double xb;
      if (y <= y1 && y1 != y0) {
        xb = x0 + (double)(x1 - x0) * (y - y0) / (y1 - y0);
      } else if (y1 != y2) {
        xb = x1 + (double)(x2 - x1) * (y - y1) / (y2 - y1);
      } else {
        xb = x1;
      }
      int a = (int)min(xa, xb);
      int b = (int)max(xa, xb);
      drawFastHLine(a, y, b - a + 1, color);
    }
  }

  void pushImage(int x, int y, int w, int h, const uint16_t* data) {
    for (int row = 0; row < h; row++) {
      for (int col = 0; col < w; col++) {
        drawPixel(x + col, y + row, data[row * w + col]);
      }
    }
  }

  // text
  void setFreeFont(const GFXfont* f) { m_font = f; }
  void setTextColor(uint32_t fg) { m_fg = (uint16_t)fg; }
  void setTextColor(uint32_t fg, uint32_t) { m_fg = (uint16_t)fg; }
  void setTextDatum(uint8_t d) { m_datum = d; }
  void setTextSize(int s) { m_size = max(1, s); }

  int textWidth(const char* s) {
    if (!m_font) {
      return 0;
    }
    int w = 0;
    for (const char* p = s; *p; p++) {
      auto g = glyphFor(*p);
      if (g) {
        w += g->xAdvance * m_size;
      }
    }
    return w;
  }

  int drawString(const char* s, int x, int y) {
    if (!m_font) {
      return 0;
    }
    auto metrics = fontMetrics(m_font);
    int w = textWidth(s);
    int h = (metrics.first + metrics.second) * m_size;

    // horizontal: datums cycle left, center, right
    if (m_datum == TC_DATUM || m_datum == MC_DATUM || m_datum == BC_DATUM) {
      x -= w / 2;
    } else if (m_datum == TR_DATUM || m_datum == MR_DATUM || m_datum == BR_DATUM) {
      x -= w;
    }

    // vertical: top, middle, bottom relative to the ascent box
    int baseline = y + metrics.first * m_size;
    if (m_datum == ML_DATUM || m_datum == MC_DATUM || m_datum == MR_DATUM) {
      baseline = y - h / 2 + metrics.first * m_size;
    } else if (m_datum == BL_DATUM || m_datum == BC_DATUM || m_datum == BR_DATUM) {
      baseline = y - h + metrics.first * m_size;
    }

    int cx = x;
    for (const char* p = s; *p; p++) {
      cx += drawGlyph(*p, cx, baseline);
    }
    return w;
  }

 protected:
  void resize(int w, int h) {
    m_w = w;
    m_h = h;
    m_buf.assign((size_t)max(1, w * h), 0);
  }

  // one filled or stroked quarter circle, corner: 1 tl, 2 tr, 4 br, 8 bl
  void circleQuadrant(int cx, int cy, int r, int corner, bool fill, uint32_t color) {
    for (int y = 0; y <= r; y++) {
      for (int x = 0; x <= r; x++) {
        if (x * x + y * y > r * r) {
          continue;
        }
        bool edge = (x + 1) * (x + 1) + y * y > r * r || x * x + (y + 1) * (y + 1) > r * r;
        if (!fill && !edge) {
          continue;
        }
        int px = corner == 1 || corner == 8 ? cx - x : cx + x;
        int py = corner == 1 || corner == 2 ? cy - y : cy + y;
        drawPixel(px, py, color);
      }
    }
  }

  const GFXglyph* glyphFor(char c) {
    if (!m_font || (uint8_t)c < m_font->first || (uint8_t)c > m_font->last) {
      return nullptr;
    }
    return &m_font->glyph[(uint8_t)c - m_font->first];
  }

  int drawGlyph(char c, int x, int baseline) {
    auto g = glyphFor(c);
    if (!g) {
      return 0;
    }
    const uint8_t* bitmap = m_font->bitmap + g->bitmapOffset;
    int bit = 0;
    for (int row = 0; row < g->height; row++) {
      for (int col = 0; col < g->width; col++) {
        if (bitmap[bit / 8] & (0x80 >> (bit % 8))) {
          int px = x + (g->xOffset + col) * m_size;
          int py = baseline + (g->yOffset + row) * m_size;
          for (int sy = 0; sy < m_size; sy++) {
            for (int sx = 0; sx < m_size; sx++) {
              drawPixel(px + sx, py + sy, m_fg);
            }
          }
        }
        bit++;
      }
    }
    return g->xAdvance * m_size;
  }

  // ascent and descent per font, cached
  std::pair<int, int> fontMetrics(const GFXfont* f) {
    static std::map<const GFXfont*, std::pair<int, int>> cache;
    auto it = cache.find(f);
    if (it != cache.end()) {
      return it->second;
    }
    int ascent = 0, descent = 0;
    for (int c = f->first; c <= f->last; c++) {
      auto& g = f->glyph[c - f->first];
      ascent = max(ascent, -(int)g.yOffset);
      descent = max(descent, (int)g.yOffset + (int)g.height);
    }
    cache[f] = {ascent, descent};
    return {ascent, descent};
  }

  std::vector<uint16_t> m_buf;
  int m_w = 0;
  int m_h = 0;
  const GFXfont* m_font = nullptr;
  uint16_t m_fg = TFT_WHITE;
  uint8_t m_datum = TL_DATUM;
  int m_size = 1;
};

class TFT_eSprite : public TFT_eSPI {
 public:
  TFT_eSprite(TFT_eSPI* parent) : TFT_eSPI(1, 1), m_parent{parent} {}

  void* createSprite(int w, int h) {
    resize(w, h);
    return m_buf.data();
  }

  void fillSprite(uint32_t color) { fillScreen(color); }

  void pushSprite(int x, int y) { m_parent->pushImage(x, y, m_w, m_h, m_buf.data()); }

  uint16_t* getPointer() { return m_buf.data(); }

 private:
  TFT_eSPI* m_parent;
};
