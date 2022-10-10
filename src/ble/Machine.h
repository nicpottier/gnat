#pragma once

namespace ble {

class Machine {
 public:
  virtual bool sleep() = 0;
  virtual bool stop() = 0;
};

}