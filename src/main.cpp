#include <Arduino.h>
#include <Command.h>
#include <Config.h>
#include <Data.h>

#ifdef M5_STICK
#include <M5StickCPlus.h>
#else
#include <TFT_eSPI.h>
#endif

#include <AsyncTCP.h>
#include <DNSServer.h>
#include <Esp.h>
#include <NimBLEDevice.h>
#include <Screen.h>
#include <WiFi.h>
#include <assets.h>
#include <ble/DE1.h>
#include <ble/FoundDevice.h>
#include <ble/Skale.h>
#include <widget/BrewBackground.h>
#include <widget/ConfigBackground.h>
#include <widget/MachineStatus.h>
#include <widget/ScaleStatus.h>
#include <widget/ShotGraph.h>
#include <widget/ShotTimer.h>
#include <widget/Splash.h>

#include "ESPAsyncWebServer.h"

#ifndef SSID
#define SSID "GNAT"
#endif

DNSServer dnsServer;
AsyncWebServer server(80);

// the tft we draw to
TFT_eSPI tft = TFT_eSPI(135, 240);

// The build version comes from an environment variable. Use the VERSION
// define wherever the version is needed.
#define STRINGIZER(arg) #arg
#define STR_VALUE(arg) STRINGIZER(arg)
#define VERSION STR_VALUE(BUILD_VERSION)

static NimBLEScan* pBLEScan;
static ble::Device* devices[2];

static Config config{};

static QueueHandle_t foundDeviceQ;

static QueueHandle_t updateQ;
static QueueHandle_t cmdQ;

static Screen* s_splashScreen;
static Screen* s_brewScreen;
static Screen* s_configScreen;

const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;

auto ctx = data::Context{};

class BLEAdCallback : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* d) {
    Serial.printf("[%d][BLE] new ble ad: %s \n", xPortGetCoreID(), d->toString().c_str());

    for (auto bled : devices) {
      // if we are already connected, move on
      if (!bled->isDisconnected()) {
        continue;
      }
      auto connect = bled->shouldConnect(d);

      // if we should connect, add it to our queue
      if (connect) {
        auto found = ble::FoundDevice{bled, d->getAddress()};
        if (xQueueSend(foundDeviceQ, &found, 10) != pdTRUE) {
          Serial.printf("[%d][BLE] ERROR: Could not add found address to queue: %s\n", xPortGetCoreID(),
                        d->getAddress().toString().c_str());
        } else {
          Serial.printf("[BLE] Added %s to queue to connect\n", d->getAddress().toString().c_str());
        }
      }
    }
  };
};

ble::Skale* skale = nullptr;
ble::DE1* de1 = nullptr;

void bleLoop(void* parameters) {
  Serial.printf("[%d] BLE Loop started\n", xPortGetCoreID());

  auto d = ble::FoundDevice{};
  auto c = cmd::CommandRequest{cmd::CommandType::EMPTY};
  auto devices = new ble::Devices{};

  while (1) {
    // do we have anything to connect to?
    while (xQueueReceive(foundDeviceQ, (void*)&d, 0) == pdTRUE) {
      auto device = d.getDevice();

      // if our device is already connected, move on
      if (!device->isDisconnected()) {
        Serial.printf("[BLE] Ignoring connection request for %s, not disconnected\n",
                      d.getAddress().toString().c_str());
        continue;
      }

      auto client = NimBLEDevice::createClient(d.getAddress());

      /** Set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout.
       *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
       *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
       *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
       */
      client->setConnectionParams(12, 12, 0, 51);

      // Set how long we are willing to wait for the connection to complete (seconds), default is 30.
      client->setConnectTimeout(5);

      Serial.printf("[BLE] Connecting to %s\n", d.getAddress().toString().c_str());

      // connect, save our device type if we succeed
      if (device->connect(client)) {
        device->selfRegister(devices);
      } else {
        Serial.printf("[BLE] Failed to connect client for %s:%d\n", device->getName().c_str(),
                      device->isDisconnected());

        // failed to connect, delete the client, we'll try again later
        NimBLEDevice::deleteClient(client);
      }
    }

    // apply any device commands
    cmd::CommandType lastCmdType = cmd::CommandType::EMPTY;
    while (xQueueReceive(cmdQ, (void*)&c, 0) == pdTRUE) {
      // collapse repeat commands
      if (c.getType() == lastCmdType) {
        continue;
      }

      lastCmdType = c.getType();
      if (!c.execute(devices)) {
        Serial.printf("failed executing command\n");
      }
    }

    // check whether our scan needs restarting
    if (!pBLEScan->isScanning()) {
      pBLEScan->start(0, nullptr, false);
    }

    // sleep a sec
    vTaskDelay(40 / portTICK_PERIOD_MS);
  }
}

bool startingWifi = false;

class CaptiveRequestHandler : public AsyncWebHandler {
 public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest* request) override {
    return true;
  }

  bool isRequestHandlerTrivial() {
    return false;
  }

  void handleRequest(AsyncWebServerRequest* request) override {
    Serial.printf("%s: %s\n", request->methodToString(), request->url().c_str());

    if (request->url() == "/gnat_white.png") {
      auto response = request->beginResponse_P(200, "image/png", gnat_white_png, gnat_white_png_len);
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
      Serial.println("sent gnat png");
    }

    else if (request->url() == "/pico.min.css") {
      auto response = request->beginResponse_P(200, "text/css", pico_min_css, pico_min_css_len);
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
      Serial.println("sent css");
    }

    else if (request->url() == "/config") {
      if (request->method() == HTTP_POST) {
        auto newConfig = Config::fromRequest(request);

        // no errors? save our new config
        if (newConfig.getError() == ConfigError::none) {
          writeConfig(newConfig);
          auto response = request->beginResponse_P(200, "text/html", success_html, success_html_len);
          response->addHeader("Content-Encoding", "gzip");
          request->send(response);

          auto restart = data::DataUpdate::newRestartUpdate();
          if (xQueueSend(updateQ, &restart, 10) != pdTRUE) {
            Serial.println("Error queueing restart update");
          }
        } else {
          char query[64];
          newConfig.toURLQuery(query, 64);

          char url[128];
          snprintf(url, 128, "/config?%s", query);
          request->redirect(url);
          Serial.println("redirected to config");
        }
      } else {
        auto response = request->beginResponse_P(200, "text/html", index_html, index_html_len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
        Serial.println("sent config");
      }
    }

    else {
      char query[64];
      config.toURLQuery(query, 64);

      char url[128];
      snprintf(url, 128, "/config?%s", query);
      request->redirect(url);
      Serial.println("redirected to config");
    }
  }
};

long lastMenuPress = millis();

void IRAM_ATTR menuButtonPressed() {
  // debounce
  if (millis() - lastMenuPress > 1000) {
    auto screen = data::DataUpdate::newScreenUpdate((ctx.screen == ScreenID::brew) ? ScreenID::config : ScreenID::brew);
    if (xQueueSend(updateQ, &screen, 10) != pdTRUE) {
      Serial.println("error queuing screen update");
    }
  }
  lastMenuPress = millis();
}

void setup() {
#ifdef M5_STICK
  M5.begin();
#endif

  Serial.begin(115200);
  Serial.printf("[%d] Setup - Version: %s\n", xPortGetCoreID(), VERSION);

  sleep(1);

  config = readConfig();

  foundDeviceQ = xQueueCreate(10, sizeof(ble::FoundDevice));
  updateQ = xQueueCreate(100, sizeof(data::DataUpdate));
  cmdQ = xQueueCreate(100, sizeof(cmd::CommandRequest));

  skale = new ble::Skale{updateQ, cmdQ};
  devices[0] = skale;
  de1 = new ble::DE1{updateQ, cmdQ};
  devices[1] = de1;

  s_splashScreen = new Screen{ScreenID::splash};
  s_splashScreen->addWidget(new widget::Splash{240, 135});

  s_brewScreen = new Screen{ScreenID::brew};
  s_brewScreen->addWidget(new widget::BrewBackground{240, 135});
  s_brewScreen->addWidget(new widget::ScaleStatus{5, 7, 80});
  s_brewScreen->addWidget(new widget::MachineStatus{85, 7, 80});
  s_brewScreen->addWidget(new widget::ShotTimer{165, 7, 80});
  s_brewScreen->addWidget(new widget::ShotGraph{5, 40, 230, 90});

  s_configScreen = new Screen{ScreenID::config};
  s_configScreen->addWidget(new widget::ConfigBackground{240, 135});

  /** *Optional* Sets the filtering mode used by the scanner in the BLE
   * controller.
   *
   *  Can be one of:
   *  CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE (0) (default)
   *  Filter by device address only, advertisements from the same address will
   * be reported only once.
   *
   *  CONFIG_BTDM_SCAN_DUPL_TYPE_DATA (1)
   *  Filter by data only, advertisements with the same data will only be
   * reported once, even from different addresses.
   *
   *  CONFIG_BTDM_SCAN_DUPL_TYPE_DATA_DEVICE (2)
   *  Filter by address and data, advertisements from the same address will be
   * reported only once, except if the data in the advertisement has changed,
   * then it will be reported again.
   *
   *  Can only be used BEFORE calling NimBLEDevice::init.
   */
  NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DATA_DEVICE);

  /** *Optional* Sets the scan filter cache size in the BLE controller.
   *  When the number of duplicate advertisements seen by the controller
   *  reaches this value it will clear the cache and start reporting previously
   *  seen devices. The larger this number, the longer time between repeated
   *  device reports. Range 10 - 1000. (default 20)
   *
   *  Can only be used BEFORE calling NimBLEDevice::init.
   */
  NimBLEDevice::setScanDuplicateCacheSize(10);

  NimBLEDevice::init("");

  pBLEScan = NimBLEDevice::getScan();  // create new scan

  // Set the callback for when devices are discovered, no duplicates.
  pBLEScan->setAdvertisedDeviceCallbacks(new BLEAdCallback(), false);

  // Set active scanning, this will get more data from the advertiser.
  // pBLEScan->setActiveScan(true);

  // How often the scan occurs / switches channels; in milliseconds,
  pBLEScan->setInterval(97);

  // How long to scan during the interval; in milliseconds.
  pBLEScan->setWindow(37);

  // do not store the scan results, use callback only.
  pBLEScan->setMaxResults(0);

  xTaskCreatePinnedToCore(bleLoop, "BLE Loop", 4096, NULL, 1, NULL, 0);

#ifdef M5_STICK
  M5.Axp.ScreenBreath(7);
  tft = M5.Lcd;
#else
  ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution);
  ledcAttachPin(TFT_BL, pwmLedChannelTFT);
  ledcWrite(pwmLedChannelTFT, 100);

  tft.init();
#endif

  pinMode(MENU_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(MENU_BUTTON_PIN, menuButtonPressed, FALLING);

  tft.setRotation(3);
  tft.setSwapBytes(true);
  tft.fillScreen(theme.bg_color);

  // your other setup stuff...
  WiFi.softAP("GNAT");
  if (dnsServer.start(53, "*", WiFi.softAPIP())) {
    Serial.println("DNS Server Started");
  } else {
    Serial.println("unable to start dns server");
  }
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);  // only when requested from AP

  // more handlers...
  server.begin();

  // show our splash for 2 seconds
  ctx.splashEnd = millis() + 2000;
}

unsigned long lastTick = 0;
unsigned long idleStart = 0;

unsigned long lastTare = 0;
unsigned long lastSleep = 0;
unsigned long lastStop = 0;

// how often we are trying to paint in millis, ~50 fps
const unsigned long TICK_TARGET = 20;

// how long in millis before we put the machine to sleep
const unsigned long SLEEP_TIMEOUT = 1000 * 60 * 15;

// how many ticks before we try a command again, default 2 seconds
const unsigned long CMD_TIMEOUT = 2000 / TICK_TARGET;

void loop() {
  // container for the commands we pop off our queue
  auto d = data::DataUpdate{UpdateType::null_update};
  auto nextTick = 0;
  idleStart = millis();

  auto lastState = MachineState::unknown;
  auto lastSubstate = MachineSubstate::unknown;

  while (true) {
    while (xQueueReceive(updateQ, (void*)&d, 0) == pdTRUE) {
      d.apply(&ctx);
    }

    // ready for our next tick?
    if (millis() > nextTick) {
      // paint all our screens
      s_splashScreen->tickAndPaint(ctx, tft);
      s_brewScreen->tickAndPaint(ctx, tft);
      s_configScreen->tickAndPaint(ctx, tft);

      // we just went to sleep, turn off the screen
      if (ctx.machineState == MachineState::sleep && lastState != ctx.machineState) {
#ifdef M5_STICK
        M5.Axp.ScreenSwitch(false);
#endif
      }

      // just left sleep, turn on screen
      if ((lastState == MachineState::sleep || lastState == MachineState::unknown) &&
          ctx.machineState != MachineState::sleep) {
#ifdef M5_STICK
        M5.Axp.ScreenSwitch(true);
#endif
        idleStart = millis();
      }

      // if we just switched into brewing espresso, tare our scale
      if ((ctx.machineState != lastState && ctx.machineState == MachineState::espresso) ||
          (ctx.machineState == MachineState::espresso && lastSubstate < MachineSubstate::preinfusing &&
           ctx.machineSubstate >= MachineSubstate::preinfusing)) {
        if (ctx.tickID - lastTare > CMD_TIMEOUT) {
          auto tare = cmd::CommandRequest::newTareScaleCommand();
          xQueueSend(cmdQ, &tare, 10);
          lastTare = ctx.tickID;
        }
      }

      // if we are brewing and we are over 36grams, stop
      if (ctx.machineState == MachineState::espresso && ctx.machineSubstate == MachineSubstate::pouring &&
          ctx.currentWeight > 35) {
        if (ctx.tickID - lastStop > CMD_TIMEOUT) {
          auto stop = cmd::CommandRequest::newStopMachineCommand();
          xQueueSend(cmdQ, &stop, 10);
          lastStop = ctx.tickID;
        }
      }

      // switching into idle, reset our timeout
      if (ctx.machineState == MachineState::idle && lastState != MachineState::idle) {
        idleStart = millis();
      }

      // if we haven't brewed anything for a bit, sleep
      if (ctx.machineState == MachineState::idle && millis() - idleStart > SLEEP_TIMEOUT) {
        if (ctx.tickID - lastSleep > CMD_TIMEOUT) {
          auto sleep = cmd::CommandRequest::newSleepMachineCommand();
          xQueueSend(cmdQ, &sleep, 10);
          lastSleep = ctx.tickID;
        }
      }

      // if it's time to reboot, do so
      if (ctx.restartTickID > 0 && ctx.tickID > ctx.restartTickID) {
        ESP.restart();
      }

      // splash is complete, move to brew screen
      if (ctx.screen == ScreenID::splash && millis() > ctx.splashEnd) {
        auto screen = data::DataUpdate::newScreenUpdate(ScreenID::brew);
        if (xQueueSend(updateQ, &screen, 10) != pdTRUE) {
          Serial.println("error queuing screen update");
        }
      }

      lastState = ctx.machineState;
      lastSubstate = ctx.machineSubstate;

      // increment our tick id and save our last tick
      ctx.tickID++;
      lastTick = millis();
      nextTick = lastTick + TICK_TARGET;

      dnsServer.processNextRequest();
    }
  }
}
