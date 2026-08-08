#pragma once

// touch controller shim fed by the host's mouse and gesture buttons

#include "../emu_bridge.h"

class CST816 {
 public:
  bool begin(int, int) { return true; }

  bool read(int& x, int& y) {
    if (!emu::touchDown) {
      return false;
    }
    x = emu::touchX;
    y = emu::touchY;
    return true;
  }
};
