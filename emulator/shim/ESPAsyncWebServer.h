#pragma once

// the captive portal never serves in the emulator, these stubs just satisfy
// the compiler

#include "Arduino.h"

#define HTTP_POST 1

class AsyncWebParameter {
 public:
  String value() { return String(""); }
};

class AsyncWebServerResponse {
 public:
  void addHeader(const char*, const char*) {}
};

class AsyncResponseStream : public AsyncWebServerResponse {
 public:
  void print(const char*) {}
  void printf(const char*, ...) {}
};

class AsyncWebServerRequest {
 public:
  const char* methodToString() { return "GET"; }
  String url() { return String("/"); }
  int method() { return 0; }
  AsyncWebParameter* getParam(const char*, bool = false, bool = false) { return nullptr; }
  AsyncWebServerResponse* beginResponse_P(int, const char*, const uint8_t*, unsigned int) { return &m_response; }
  AsyncWebServerResponse* beginResponse_P(int, const char*, const char*) { return &m_response; }
  AsyncResponseStream* beginResponseStream(const char*) { return &m_stream; }
  void send(AsyncWebServerResponse*) {}
  void send(int, const char*, const char*) {}
  void redirect(const char*) {}

 private:
  AsyncWebServerResponse m_response;
  AsyncResponseStream m_stream;
};

class AsyncWebHandler {
 public:
  virtual ~AsyncWebHandler() {}
  virtual bool canHandle(AsyncWebServerRequest*) { return false; }
  virtual void handleRequest(AsyncWebServerRequest*) {}
  AsyncWebHandler& setFilter(bool (*)(AsyncWebServerRequest*)) { return *this; }
};

#define ON_AP_FILTER (+[](AsyncWebServerRequest*) { return true; })

class AsyncWebServer {
 public:
  AsyncWebServer(int) {}
  AsyncWebHandler& addHandler(AsyncWebHandler* h) { return *h; }
  void begin() {}
  void end() {}
  void reset() {}
};
