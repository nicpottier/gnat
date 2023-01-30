#pragma once

#include <Context.h>

#ifdef M5_STICK
#include <M5Display.h>
#else
#include <TFT_eSPI.h>
#endif

namespace widget {
class Widget {
 public:
  virtual bool tick(ctx::Context ctx, unsigned long tick, unsigned long millis) = 0;
  virtual void paint(TFT_eSPI &tft) = 0;
};
}  // namespace widget