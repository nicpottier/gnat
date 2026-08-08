#pragma once

namespace ble {

class Machine {
 public:
  virtual bool sleep() = 0;
  virtual bool stop() = 0;
  virtual bool wake() = 0;
  virtual bool setRefillLevel(int mm) = 0;
  virtual bool setProfile(int idx) = 0;
  virtual bool setFlushSeconds(int seconds) = 0;
  virtual bool setShotSettings(int steamTemp, int steamSeconds, int waterTemp, int waterVol) = 0;
};

}