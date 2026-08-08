#pragma once

// the bridge between the firmware compiled against the shims and the SDL
// host: framebuffer, synthetic touch, pins and the firmware's queues

#include <atomic>
#include <cstdint>
#include <mutex>

// the emulated panel in landscape orientation, matching the amoled
const int emu_panel_w = 536;
const int emu_panel_h = 240;

namespace emu {

// the panel contents as pushed by the display driver shim
extern uint16_t framebuffer[emu_panel_w * emu_panel_h];
extern std::mutex fbMutex;
extern std::atomic<bool> displayOn;
extern std::atomic<long> fbGeneration;

// synthetic touch state in controller coordinate space, the CST816 shim
// reports these verbatim
extern std::atomic<bool> touchDown;
extern std::atomic<int> touchX;
extern std::atomic<int> touchY;

// gpio pins with interrupt dispatch, pull ups default high
void setPin(int pin, int level);
int getPin(int pin);
void registerInterrupt(int pin, void (*fn)(), int mode);

// the firmware's queues in creation order: found devices, updates, commands
void* queueAt(int idx);
void registerQueue(void* q);

// set when the firmware asks for a restart
extern std::atomic<bool> restartRequested;

}  // namespace emu
