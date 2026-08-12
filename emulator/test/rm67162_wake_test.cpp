// driver-level test for the amoled wake path.
//
// symptom: right after a boot power cycle the panel is put to sleep (GNAT
// connects to a still-asleep machine) and then, when the machine is turned
// on, the panel comes back but stays dark. bare sleep-out + display-on isn't
// enough to revive a freshly power-cycled panel; it needs its full display
// setup (pixel format + brightness + display-on) re-asserted.
//
// the emulator stubs the whole rm67162 driver, so this compiles the real
// src/amoled/rm67162.cpp against a fake SPI bus and checks that lcd_wake
// re-establishes the panel to the same visible state rm67162_init does.

#include <cstdio>

#include "driver/spi_master.h"  // fake bus + recorded command log

#include "rm67162.h"

enum {
  CMD_SLEEP_IN = 0x10,
  CMD_SLEEP_OUT = 0x11,
  CMD_PIXEL_FORMAT = 0x3A,
  CMD_DISPLAY_OFF = 0x28,
  CMD_DISPLAY_ON = 0x29,
  CMD_BRIGHTNESS = 0x51,
};

// a small model of the panel's visible state, advanced from the command
// stream. a freshly power-cycled panel is dark until its brightness is set,
// which is why init writes 0x51 last; sleep drops that state again.
struct Panel {
  bool sleeping = true;
  bool displayOn = false;
  int brightness = 0;

  void apply(const FakeSpiCommand& c) {
    switch (c.cmd) {
      case CMD_SLEEP_IN: sleeping = true; brightness = 0; break;
      case CMD_SLEEP_OUT: sleeping = false; break;
      case CMD_DISPLAY_OFF: displayOn = false; break;
      case CMD_DISPLAY_ON: displayOn = true; break;
      case CMD_BRIGHTNESS: brightness = c.data.empty() ? 0 : c.data[0]; break;
    }
  }
  bool visible() const { return !sleeping && displayOn && brightness > 0; }
};

static int failures = 0;
static void check(const char* what, bool cond) {
  printf("  %-52s %s\n", what, cond ? "ok" : "FAIL");
  if (!cond) failures++;
}

static Panel replay(Panel p = {}) {
  for (auto& c : fakeSpiLog()) p.apply(c);
  return p;
}

// did the last recorded stream contain this command with non-zero data?
static bool sentNonZero(uint8_t cmd) {
  for (auto& c : fakeSpiLog())
    if (c.cmd == cmd && !c.data.empty() && c.data[0] != 0) return true;
  return false;
}
static bool sent(uint8_t cmd) {
  for (auto& c : fakeSpiLog())
    if (c.cmd == cmd) return true;
  return false;
}

int main() {
  // baseline: a fresh init leaves the panel visible (validates the model)
  fakeSpiLog().clear();
  rm67162_init();
  Panel afterInit = replay();
  check("panel is visible after init", afterInit.visible());

  // sleep turns it off
  fakeSpiLog().clear();
  lcd_sleep();
  Panel afterSleep = replay(afterInit);
  check("panel is dark after lcd_sleep", !afterSleep.visible());

  // wake must fully re-establish the display, not just sleep-out + display-on
  fakeSpiLog().clear();
  lcd_wake();
  Panel afterWake = replay(afterSleep);
  check("lcd_wake sends sleep-out", sent(CMD_SLEEP_OUT));
  check("lcd_wake re-asserts pixel format", sent(CMD_PIXEL_FORMAT));
  check("lcd_wake sends display-on", sent(CMD_DISPLAY_ON));
  check("lcd_wake restores non-zero brightness", sentNonZero(CMD_BRIGHTNESS));
  check("panel is visible after lcd_wake", afterWake.visible());

  printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "PASSED", failures);
  return failures ? 1 : 0;
}
