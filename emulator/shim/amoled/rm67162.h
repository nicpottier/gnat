#pragma once

// display driver shim: pushes land in the shared emulator framebuffer

#include <cstring>

#include "../emu_bridge.h"

inline void rm67162_init() {}

inline void lcd_setRotation(int) {}

inline void lcd_sleep() {
  emu::displayOn = false;
  emu::fbGeneration++;
}

inline void lcd_wake() {
  emu::displayOn = true;
  emu::fbGeneration++;
}

inline void lcd_PushColors(int x, int y, int w, int h, uint16_t* data) {
  std::lock_guard<std::mutex> lock(emu::fbMutex);
  for (int row = 0; row < h; row++) {
    int dy = y + row;
    if (dy < 0 || dy >= emu_panel_h) {
      continue;
    }
    for (int col = 0; col < w; col++) {
      int dx = x + col;
      if (dx < 0 || dx >= emu_panel_w) {
        continue;
      }
      emu::framebuffer[dy * emu_panel_w + dx] = data[row * w + col];
    }
  }
  emu::fbGeneration++;
}
