#pragma once

// ble never runs in the emulator, the host simulates the machine and scale
// by feeding the firmware's own queues, these stubs satisfy the compiler

#include <functional>
#include <string>
#include <vector>

#include "Arduino.h"

#define CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE 0
#define CONFIG_BTDM_SCAN_DUPL_TYPE_DATA 1
#define CONFIG_BTDM_SCAN_DUPL_TYPE_DATA_DEVICE 2

class NimBLEAddress {
 public:
  NimBLEAddress() {}
  NimBLEAddress(const char* s) : m_s{s} {}
  NimBLEAddress(const std::string& s) : m_s{s} {}
  std::string toString() const { return m_s; }

 private:
  std::string m_s;
};

class NimBLEUUIDish {
 public:
  std::string toString() const { return ""; }
};

class NimBLEAttValueish {
 public:
  unsigned long length() const { return 0; }
  const char* data() const { return ""; }
  const char* c_str() const { return ""; }
};

class NimBLERemoteCharacteristic {
 public:
  using notify_callback = std::function<void(NimBLERemoteCharacteristic*, uint8_t*, size_t, bool)>;

  NimBLEUUIDish getUUID() { return {}; }
  bool canRead() { return false; }
  bool canNotify() { return false; }
  NimBLEAttValueish readValue() { return {}; }
  bool subscribe(bool, notify_callback, bool = false) { return true; }

  template <typename... A>
  bool writeValue(A...) {
    return true;
  }
};

class NimBLERemoteService {
 public:
  NimBLEUUIDish getUUID() { return {}; }
  std::vector<NimBLERemoteCharacteristic*>* getCharacteristics(bool) { return &m_chars; }

 private:
  std::vector<NimBLERemoteCharacteristic*> m_chars;
};

class NimBLEClient;

class NimBLEClientCallbacks {
 public:
  virtual ~NimBLEClientCallbacks() {}
  virtual void onConnect(NimBLEClient*) {}
  virtual void onDisconnect(NimBLEClient*) {}
};

class NimBLEClient {
 public:
  void setClientCallbacks(NimBLEClientCallbacks*, bool) {}
  bool connect() { return false; }
  bool isConnected() { return false; }
  NimBLEAddress getPeerAddress() { return {}; }
  void setConnectionParams(int, int, int, int) {}
  void setConnectTimeout(int) {}
  int getConnId() { return 0; }
  std::vector<NimBLERemoteService*>* getServices(bool) { return &m_services; }

 private:
  std::vector<NimBLERemoteService*> m_services;
};

class NimBLEAdvertisedDevice {
 public:
  std::string getName() { return ""; }
  NimBLEAddress getAddress() { return {}; }
  std::string toString() { return ""; }
};

class NimBLEAdvertisedDeviceCallbacks {
 public:
  virtual ~NimBLEAdvertisedDeviceCallbacks() {}
  virtual void onResult(NimBLEAdvertisedDevice*) {}
};

class NimBLEScan {
 public:
  void setAdvertisedDeviceCallbacks(NimBLEAdvertisedDeviceCallbacks*, bool) {}
  void setInterval(int) {}
  void setWindow(int) {}
  void setMaxResults(int) {}
  void setActiveScan(bool) {}
  bool isScanning() { return true; }
  void start(int, void*, bool) {}
};

class NimBLEDevice {
 public:
  static void init(const std::string&) {}
  static NimBLEScan* getScan() {
    static NimBLEScan scan;
    return &scan;
  }
  static NimBLEClient* createClient(NimBLEAddress) { return new NimBLEClient(); }
  static void deleteClient(NimBLEClient* c) { delete c; }
  static void setScanFilterMode(int) {}
  static void setScanDuplicateCacheSize(int) {}
};
