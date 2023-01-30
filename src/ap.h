#pragma once

#include <AsyncTCP.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <assets.h>

#include "Config.h"
#include "Context.h"
#include "ESPAsyncWebServer.h"

#ifndef SSID
#define SSID "GNAT"
#endif

DNSServer g_dnsServer;
AsyncWebServer g_server(80);

bool apOn = false;

class CaptiveRequestHandler : public AsyncWebHandler {
 public:
  CaptiveRequestHandler(ctx::Context* ctx, QueueHandle_t updateQ)
      : m_updateQ(updateQ),
        m_ctx(ctx) {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest* request) override {
    return true;
  }

  bool isRequestHandlerTrivial() {
    return false;
  }

  void sendConfigRedirect(AsyncWebServerRequest* request, Config config, const char* msg) {
    char query[255];
    config.toURLQuery(query, 255);

    char url[500];
    snprintf(url, 500, "/config?%s&msg=%s", query, msg);
    request->redirect(url);
  }

  void handleRequest(AsyncWebServerRequest* request) override {
    auto url = request->url().c_str();
    Serial.printf("[%d] %s: %s\n", xPortGetCoreID(), request->methodToString(), url);

    // make sure our config screen is shown on any request
    if (m_ctx->screen == ScreenID::connect) {
      auto screen = ctx::ContextUpdate::newScreenUpdate(ScreenID::config);
      if (xQueueSend(m_updateQ, &screen, 10) != pdTRUE) {
        Serial.println("error queuing screen update");
      }
    }

    if (strstr(url, "/gnat_white.png") == url) {
      auto response = request->beginResponse_P(200, "image/png", gnat_white_png, gnat_white_png_len);
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    }

    else if (strstr(url, "/pico.min.css") == url) {
      auto response = request->beginResponse_P(200, "text/css", pico_min_css, pico_min_css_len);
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    }

    else if (strstr(url, "/config") == url) {
      if (request->method() == HTTP_POST) {
        auto newConfig = Config::fromRequest(request);

        // no errors? save our new config
        if (newConfig.getError() == ConfigError::none) {
          writeConfig(newConfig);
          // queue our config update
          auto configUpdate = ctx::ContextUpdate::newConfigUpdate(newConfig);
          if (xQueueSend(m_updateQ, &configUpdate, 10) != pdTRUE) {
            Serial.println("Error queueing config update");
          }
          sendConfigRedirect(request, newConfig, "Configuration+Saved");
        } else {
          sendConfigRedirect(request, newConfig, "");
        }
      } else {
        auto response = request->beginResponse_P(200, "text/html", config_html, config_html_len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
      }
    }

    else {
      sendConfigRedirect(request, m_ctx->config, "");
    }
  }

 private:
  QueueHandle_t m_updateQ;
  ctx::Context* m_ctx;
};

void startAP(ctx::Context* ctx, QueueHandle_t updateQ) {
  // bring up our GNAT AP
  WiFi.softAP("GNAT");

  // start our DNS server
  if (!g_dnsServer.start(53, "*", WiFi.softAPIP())) {
    Serial.println("unable to start dns server");
  }

  // add our handler
  g_server.addHandler(new CaptiveRequestHandler(ctx, updateQ)).setFilter(ON_AP_FILTER);  // only when requested from AP

  // start handling
  g_server.begin();

  apOn = true;
}

void stopAP() {
  apOn = false;

  // stop handling
  g_server.end();

  // clear all handlers
  g_server.reset();

  // turn off dns
  g_dnsServer.stop();

  // turn off our AP
  WiFi.softAPdisconnect(true);
}

void tickAP() {
  if (apOn) {
    g_dnsServer.processNextRequest();
  }
}
