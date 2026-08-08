#pragma once

#include <widget/splash_sprite.h>

#ifdef M5_STICK
#include <M5Display.h>
#else
#include <TFT_eSPI.h>
#endif

namespace widget {

// pushImage isn't virtual, when rendering into a framebuffer sprite we have
// to call the sprite's version so it doesn't touch the (unused) tft bus
inline void pushLogoRow(TFT_eSPI& tft, int x, int y, int w, uint16_t* row) {
#ifdef DISPLAY_RM67162
  ((TFT_eSprite&)tft).pushImage(x, y, w, 1, row);
#else
  tft.pushImage(x, y, w, 1, row);
#endif
}

// linear blend between two RGB565 colors, t 0.0 - 1.0
inline uint16_t lerp565(uint16_t a, uint16_t b, float t) {
  int ar = a >> 11, ag = (a >> 5) & 0x3f, ab = a & 0x1f;
  int br = b >> 11, bg = (b >> 5) & 0x3f, bb = b & 0x1f;
  int r = ar + int((br - ar) * t);
  int g = ag + int((bg - ag) * t);
  int bl = ab + int((bb - ab) * t);
  return uint16_t((r << 11) | (g << 5) | bl);
}

// draws the logo at x,y scaled up by an integer factor for dense panels, the
// sprite is pure black and white so each row expands into a scaled buffer
inline void drawLogo(TFT_eSPI& tft, int x, int y, int scale) {
  if (scale < 1) {
    scale = 1;
  }
  if (scale > 4) {
    scale = 4;
  }

  uint16_t row[splash_sprite_width * 4];
  for (int sy = 0; sy < (int)splash_sprite_height; sy++) {
    for (int sx = 0; sx < (int)splash_sprite_width; sx++) {
      auto c = splash_sprite[sy * splash_sprite_width + sx];
      for (int i = 0; i < scale; i++) {
        row[sx * scale + i] = c;
      }
    }
    for (int i = 0; i < scale; i++) {
      pushLogoRow(tft, x, y + sy * scale + i, splash_sprite_width * scale, row);
    }
  }
}

}  // namespace widget
