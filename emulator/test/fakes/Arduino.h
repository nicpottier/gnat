#pragma once

// tiny Arduino stand-in for the driver-level rm67162 test: just the pin,
// timing and allocation surface the driver touches. delay is a no-op so the
// panel reset dance doesn't slow the test down.

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define PROGMEM
#define IRAM_ATTR

#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return HIGH; }
inline void delay(unsigned long) {}
inline unsigned long millis() { return 0; }

inline void* ps_malloc(size_t n) { return malloc(n); }

class SerialClass {
 public:
  void begin(long) {}
  void printf(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    vprintf(fmt, a);
    va_end(a);
  }
  void print(const char*) {}
  void println(const char*) {}
  void println() {}
};
inline SerialClass Serial;
