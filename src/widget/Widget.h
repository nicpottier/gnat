#pragma once

#include <Data.h>
#include <M5Display.h>

namespace widget {
class Widget {
 public:
  virtual bool tick(data::Context ctx, long tick, long millis) = 0;
  virtual void paint(TFT_eSPI &tft) = 0;
};
}  // namespace widget