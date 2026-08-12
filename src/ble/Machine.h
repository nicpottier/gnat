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
  virtual bool pourWater(int temp, int vol) = 0;

  // re-read the machine's state and surface it if it moved since we last saw
  // it, so a dropped state notification (which can otherwise strand us on a
  // stale state, e.g. a missed wake leaving the display asleep) self heals
  virtual bool refreshState() { return false; }
};

}