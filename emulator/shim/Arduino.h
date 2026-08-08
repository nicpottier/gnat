#pragma once

// desktop stand-in for the arduino / esp32 environment, just enough surface
// for the gnat firmware to compile and run against the emulator host

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "emu_bridge.h"

#define PROGMEM
#define IRAM_ATTR

using std::max;
using std::min;

// pins
#define LOW 0
#define HIGH 1
#define INPUT_PULLUP 2
#define OUTPUT 3
#define CHANGE 4
#define FALLING 5
#define RISING 6

unsigned long millis();
void delay(unsigned long ms);

inline void pinMode(int, int) {}
inline void digitalWrite(int pin, int level) { emu::setPin(pin, level); }
inline int digitalRead(int pin) { return emu::getPin(pin); }
inline void attachInterrupt(int pin, void (*fn)(), int mode) { emu::registerInterrupt(pin, fn, mode); }

// backlight pwm
inline void ledcSetup(int, int, int) {}
inline void ledcAttachPin(int, int) {}
inline void ledcWrite(int, int) {}

// minimal arduino String
class String {
 public:
  String() {}
  String(const char* s) : m_s{s} {}
  String(const std::string& s) : m_s{s} {}
  const char* c_str() const { return m_s.c_str(); }
  unsigned int length() const { return (unsigned int)m_s.length(); }

 private:
  std::string m_s;
};

class SerialClass {
 public:
  void begin(long) {}
  void printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
  }
  void print(const char* s) { fputs(s, stdout); }
  void print(int v) { ::printf("%d", v); }
  void println(const char* s) { puts(s); }
  void println() { puts(""); }
};
inline SerialClass Serial;

// freertos queues, single process, mutex guarded
typedef void* QueueHandle_t;
#define pdTRUE 1
#define pdFALSE 0

QueueHandle_t xQueueCreate(int length, unsigned long itemSize);
int xQueueSend(QueueHandle_t q, const void* item, int ticks);
int xQueueSendFromISR(QueueHandle_t q, const void* item, void* woken);
int xQueueReceive(QueueHandle_t q, void* item, int ticks);

inline void vTaskDelay(int ticks) { delay(ticks); }
inline int xPortGetCoreID() { return 0; }

// tasks: the ble loop never runs in the emulator, the host plays the devices
typedef void (*TaskFunction_t)(void*);
inline void xTaskCreatePinnedToCore(TaskFunction_t, const char*, int, void*, int, void*, int) {}
