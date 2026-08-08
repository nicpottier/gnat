#include "emu_bridge.h"

#include <chrono>
#include <cstring>
#include <deque>
#include <map>
#include <thread>
#include <vector>

#include "Arduino.h"

namespace emu {

uint16_t framebuffer[emu_panel_w * emu_panel_h];
std::mutex fbMutex;
std::atomic<bool> displayOn{true};
std::atomic<long> fbGeneration{0};

std::atomic<bool> touchDown{false};
std::atomic<int> touchX{0};
std::atomic<int> touchY{0};

std::atomic<bool> restartRequested{false};

// pins default high, matching pull ups
namespace {
std::mutex pinMutex;
std::map<int, int> pinLevels;
struct PinIsr {
  void (*fn)();
  int mode;
};
std::map<int, PinIsr> pinIsrs;
}  // namespace

void setPin(int pin, int level) {
  void (*fire)() = nullptr;
  {
    std::lock_guard<std::mutex> lock(pinMutex);
    auto it = pinLevels.find(pin);
    int prev = it == pinLevels.end() ? HIGH : it->second;
    pinLevels[pin] = level;
    auto isr = pinIsrs.find(pin);
    if (isr != pinIsrs.end() && prev != level) {
      if (isr->second.mode == CHANGE || (isr->second.mode == FALLING && level == LOW) ||
          (isr->second.mode == RISING && level == HIGH)) {
        fire = isr->second.fn;
      }
    }
  }
  if (fire) {
    fire();
  }
}

int getPin(int pin) {
  std::lock_guard<std::mutex> lock(pinMutex);
  auto it = pinLevels.find(pin);
  return it == pinLevels.end() ? HIGH : it->second;
}

void registerInterrupt(int pin, void (*fn)(), int mode) {
  std::lock_guard<std::mutex> lock(pinMutex);
  pinIsrs[pin] = {fn, mode};
}

// queue registry
namespace {
struct EmuQueue {
  std::mutex m;
  std::deque<std::vector<uint8_t>> items;
  size_t itemSize;
  size_t maxLen;
};
std::mutex registryMutex;
std::vector<EmuQueue*> registry;
}  // namespace

void registerQueue(void* q) {
  std::lock_guard<std::mutex> lock(registryMutex);
  registry.push_back((EmuQueue*)q);
}

void* queueAt(int idx) {
  std::lock_guard<std::mutex> lock(registryMutex);
  if (idx < 0 || idx >= (int)registry.size()) {
    return nullptr;
  }
  return registry[idx];
}

}  // namespace emu

unsigned long millis() {
  static auto start = std::chrono::steady_clock::now();
  return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
      .count();
}

void delay(unsigned long ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

QueueHandle_t xQueueCreate(int length, unsigned long itemSize) {
  auto q = new emu::EmuQueue{};
  q->itemSize = itemSize;
  q->maxLen = length;
  emu::registerQueue(q);
  return q;
}

int xQueueSend(QueueHandle_t handle, const void* item, int) {
  auto q = (emu::EmuQueue*)handle;
  std::lock_guard<std::mutex> lock(q->m);
  if (q->items.size() >= q->maxLen) {
    return pdFALSE;
  }
  std::vector<uint8_t> copy(q->itemSize);
  memcpy(copy.data(), item, q->itemSize);
  q->items.push_back(std::move(copy));
  return pdTRUE;
}

int xQueueSendFromISR(QueueHandle_t handle, const void* item, void*) {
  return xQueueSend(handle, item, 0);
}

int xQueueReceive(QueueHandle_t handle, void* item, int) {
  auto q = (emu::EmuQueue*)handle;
  std::lock_guard<std::mutex> lock(q->m);
  if (q->items.empty()) {
    return pdFALSE;
  }
  memcpy(item, q->items.front().data(), q->itemSize);
  q->items.pop_front();
  return pdTRUE;
}
