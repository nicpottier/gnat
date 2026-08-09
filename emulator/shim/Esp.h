#pragma once

#include "emu_bridge.h"

class EspClass {
 public:
  void restart() {
    emu::restartRequested = true;
    // the host notices and relaunches the process
    while (true) {
      delay(100);
    }
  }
};

inline EspClass ESP;
