#pragma once

#include "WiFi.h"

class DNSServer {
 public:
  bool start(int, const char*, IPAddress) { return true; }
  void stop() {}
  void processNextRequest() {}
};
