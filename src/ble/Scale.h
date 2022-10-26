#pragma once

namespace ble {
class Scale {
 public:
  virtual bool init() = 0;
  virtual bool tare() = 0;
  virtual bool sleep() = 0;
  virtual bool wake() = 0;
};
}