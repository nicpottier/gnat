# GNAT Emulator

Runs the real GNAT firmware on your desktop so UI changes can be exercised
without flashing a device. `src/main.cpp` and every widget compile unmodified
against a shim layer (`shim/`) that stands in for Arduino, TFT_eSPI, NimBLE,
EEPROM, WiFi and the FreeRTOS queues. The AMOLED's framebuffer path renders
into an SDL window, and the host plays the part of the DE1 and Skale by
exchanging updates and commands over the firmware's own queues — so
stop-at-weight, tare, sleep and friends all genuinely round-trip.

## Building

Requires SDL2 (`brew install sdl2`).

```
make -C emulator
./emulator/gnat-emu
```

## Using it

Two windows open: **GNAT** is the device — the panel pixel for pixel with a
strip of the device's own inputs below it — and **GNAT Machine** is the
simulated kitchen. The DE1 and scale connect automatically shortly after
boot.

Device window:

- **Mouse on the panel** is your finger: click to tap, drag to swipe.
- The strip below holds the physical button (hold it for a long press), the
  Home circle, and canned swipe left/right gestures.

Machine window:

- **The GHC cluster** mirrors the real machine: hot water on top, steam on
  the right, espresso at the bottom, flush on the left, stop in the middle.
  A button starts its flow, pressing it again (or stop) ends it. Shot runs
  a scripted pull — preinfusion, then a pour with wavy pressure and flow
  and a weight ramp — that honors stop commands from the firmware, so
  predictive stop actually stops it.
- Sleep / Idle set the machine state, DE1 / Scale toggle connections, and
  Tank nudges the water level.

Keyboard: `z` sleep, `i` idle, `s` shot, `t` steam, `w` water, `x` stop,
`d`/`c` toggle DE1/scale, `[`/`]` tank, arrows swipe, `h` home, `b` device
button, `1`-`4` zoom, `Esc` quits. The window opens at 1x (pixel for
pixel); `EMU_ZOOM=2` picks the starting zoom.

Config persists to `gnat-emu.eeprom` in the working directory, exactly as
the device would write it (delete the file for a factory reset). A firmware
restart request (orientation flip) relaunches the process.

For headless frame grabs, `EMU_SHOT=/tmp/frame.ppm@3000` writes the canvas
as a PPM once uptime passes 3000ms; add `EMU_SHOT_EXIT=1` to quit after.

## Notes and limits

- Emulates the t-display-s3-amoled profile (536x240, touch, combo button).
  Other board profiles would mainly need their own defines and layout.
- BLE and WiFi never run: the captive portal pages and /shots aren't
  reachable, and `bleLoop` is never started. The simulator feeds the same
  queues the BLE stack would.
- Fonts are the Adafruit GFX FreeSans set vendored from TFT_eSPI, so text
  metrics match the device closely but not to the pixel.
- The firmware loop runs hot on one core, same as it does on the device.
