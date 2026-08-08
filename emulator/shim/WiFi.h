#pragma once

class IPAddress {};

class WiFiClass {
 public:
  void softAP(const char*) {}
  IPAddress softAPIP() { return {}; }
  void softAPdisconnect(bool) {}
};

inline WiFiClass WiFi;
