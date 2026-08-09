# Recreating the README boot/shot GIF

The GIF at the top of README.md (`emulator/docs/boot.gif`) is recorded from
the emulator. To regenerate it after UI changes:

1. Build the emulator (needs `brew install sdl2`):

   ```
   make -C emulator
   ```

   The Makefile builds with an empty `BUILD_VERSION` so the splash shows just
   the logo. Object files depend on the Makefile, so define changes rebuild.

2. Record frames. `EMU_AUTOSHOT=<ms>` starts the recorded shot at that
   uptime, `EMU_RECORD=prefix@start-end` dumps every other render frame as
   numbered PPMs over that uptime window, `EMU_SHOT_EXIT=1` quits when done.
   Timings that work: splash ends ~6.5s, the recorded shot (29s at grinder 0)
   started at 6.8s finishes ~48.5s, so record to ~52.5s for a beat of
   finished graph. Delete stale frames and `emulator/gnat-emu.eeprom` first —
   a persisted flipped orientation or profile changes what renders.

   ```
   cd emulator && rm -f /tmp/full_*.ppm gnat-emu.eeprom
   EMU_AUTOSHOT=6800 EMU_RECORD=/tmp/full@200-52500 EMU_SHOT_EXIT=1 ./gnat-emu
   ```

3. Assemble with Pillow: boot at real speed, the shot portion at 4x (keep
   every 4th frame), real speed again for the finish. Per-frame durations are
   the recording window divided by total recorded frames (~27ms). Convert
   frames to `P` mode with an adaptive 128-color palette to keep the file
   around 200KB, write to `emulator/docs/boot.gif` with `loop=0`.

   The shape of the script: iterate sorted frames, compute each frame's
   uptime as `200 + i * window / n`, keep all frames below 6900ms and above
   48800ms, every 4th between, and save with `save_all` + `append_images` +
   a per-frame `duration` list.

4. Eyeball the result (frame peeks or the GIF itself) before committing:
   the splash must show no version text, and the shot should end on the
   completed graph around 29s / 35g.
