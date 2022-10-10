#pragma once

#include <ble/Device.h>

namespace ble {

class FoundDevice {
 public:
  FoundDevice() : m_device{nullptr}, m_address{""} {}
  FoundDevice(ble::Device* d, NimBLEAddress a) : m_device{d}, m_address{a} {}

  ble::Device* getDevice() { return m_device; }
  NimBLEAddress getAddress() { return m_address; }

 private:
  ble::Device* m_device;
  NimBLEAddress m_address;
};

}