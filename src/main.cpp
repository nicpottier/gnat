#include <Arduino.h>
#include <profiles.h>

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
#include <widget/ConfigFields.h>
#include <widget/ConnectInstructions.h>
#include <widget/FeedbackBanner.h>
#include <widget/MachineStatus.h>
#include <widget/ProfileName.h>
#include <widget/ScaleStatus.h>
#include <widget/ShotGraph.h>
#include <widget/ShotTimer.h>
#include <widget/WaterLevel.h>

#include "ESPAsyncWebServer.h"

#ifndef SSID
#define SSID "GNAT"
#endif

const uint8_t BACKLIGHT_ON = 100;

DNSServer g_dnsServer;
AsyncWebServer g_server(80);

// the tft we draw to
TFT_eSPI tft = TFT_eSPI();

// The build version comes from an environment variable. Use the VERSION
// define wherever the version is needed.
#define STRINGIZER(arg) #arg
#define STR_VALUE(arg) STRINGIZER(arg)
#define VERSION STR_VALUE(BUILD_VERSION)

const int screenWidth = TFT_HEIGHT;
const int screenHeight = TFT_WIDTH;

static NimBLEScan* pBLEScan;
static ble::Device* devices[2];

static QueueHandle_t foundDeviceQ;

static QueueHandle_t updateQ;
static QueueHandle_t cmdQ;

static Screen* s_brewScreen;
static Screen* s_connectScreen;
static Screen* s_configScreen;
static Screen* s_feedbackScreen;

const int BACKLIGHT_PWM_FREQ = 10000;
const int BACKLIGHT_PWM_RESOLUTION = 8;
const int BACKLIGHT_PWM_CHANNEL = 0;

auto g_ctx = data::Context{};

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

    // sleep a few cycles to reset the watchdog
    vTaskDelay(40);
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
    if (g_ctx.screen == ScreenID::connect) {
      auto screen = data::DataUpdate::newScreenUpdate(ScreenID::config);
      if (xQueueSend(updateQ, &screen, 10) != pdTRUE) {
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

    else if (strstr(url, "/profiles.json") == url) {
      auto response = request->beginResponse_P(200, "application/json", profiles_json);
      response->addHeader("Cache-Control", "no-store");
      request->send(response);
    }

    else if (strstr(url, "/config") == url) {
      if (request->method() == HTTP_POST) {
        auto newConfig = Config::fromRequest(request);

        // the web form doesn't manage the profile, carry ours over
        newConfig.setProfile(g_ctx.config.getProfile());

        // no errors? save our new config
        if (newConfig.getError() == ConfigError::none) {
          writeConfig(newConfig);
          // queue our config update
          auto configUpdate = data::DataUpdate::newConfigUpdate(newConfig);
          if (xQueueSend(updateQ, &configUpdate, 10) != pdTRUE) {
            Serial.println("Error queueing config update");
          }
          sendConfigRedirect(request, newConfig, "Configuration+Saved");
        } else {
          sendConfigRedirect(request, newConfig, "");
        }
      } else {
        auto response = request->beginResponse_P(200, "text/html", config_html, config_html_len);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "no-store");
        request->send(response);
      }
    }

    else {
      sendConfigRedirect(request, g_ctx.config, "");
    }
  }
};

bool g_apRunning = false;

void startAP() {
  if (g_apRunning) {
    return;
  }
  g_apRunning = true;

  // bring up our GNAT AP
  WiFi.softAP("GNAT");

  // start our DNS server
  if (!g_dnsServer.start(53, "*", WiFi.softAPIP())) {
    Serial.println("unable to start dns server");
  }

  // add our handler
  g_server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);  // only when requested from AP

  // start handling
  g_server.begin();
}

void stopAP() {
  if (!g_apRunning) {
    return;
  }
  g_apRunning = false;

  // stop handling
  g_server.end();

  // clear all handlers
  g_server.reset();

  // turn off dns
  g_dnsServer.stop();

  // turn off our AP
  WiFi.softAPdisconnect(true);
}

#ifdef PROFILE_BUTTON_PIN
long lastProfilePress = millis();

void IRAM_ATTR profileButtonPressed() {
  // ignore bounces, but only restart the window on accepted presses so
  // fast repeated presses still register
  if (millis() - lastProfilePress > 250) {
    lastProfilePress = millis();
    auto cycle = data::DataUpdate::newProfileCycleUpdate();
    xQueueSendFromISR(updateQ, &cycle, NULL);
  }
}
#endif

long lastMenuPress = millis();

void IRAM_ATTR menuButtonPressed() {
  // ignore bounces, but only restart the window on accepted presses so
  // fast repeated presses still register
  if (millis() - lastMenuPress > 250) {
    lastMenuPress = millis();
    auto nextScreen = ScreenID::brew;
    if (g_ctx.screen == ScreenID::brew) {
      nextScreen = ScreenID::connect;
    } else if (g_ctx.screen == ScreenID::connect) {
      nextScreen = ScreenID::config;
    }
    auto screen = data::DataUpdate::newScreenUpdate(nextScreen);
    xQueueSendFromISR(updateQ, &screen, NULL);
  }
}

void setup() {
#ifdef M5_STICK
  M5.begin();
#endif

  Serial.begin(115200);
  Serial.printf("[%d] Setup - Version: %s\n", xPortGetCoreID(), VERSION);

#ifdef DISABLE_SERIAL_TIMEOUT
  // turn off caching to the serial port.. S3 gets very slow if not connected
  Serial.setTxTimeoutMs(0);
#endif

  g_ctx.config = readConfig();

  foundDeviceQ = xQueueCreate(10, sizeof(ble::FoundDevice));
  updateQ = xQueueCreate(100, sizeof(data::DataUpdate));
  cmdQ = xQueueCreate(100, sizeof(cmd::CommandRequest));

  skale = new ble::Skale{updateQ, cmdQ};
  devices[0] = skale;
  de1 = new ble::DE1{updateQ, cmdQ};
  devices[1] = de1;

  // safe to set directly, our ble task hasn't started yet
  de1->setRefillLevel(g_ctx.config.getRefillLevel());
  de1->setProfile(g_ctx.config.getProfile() - 1);

  s_brewScreen = new Screen{ScreenID::brew};
  s_brewScreen->addWidget(new widget::BrewBackground{screenWidth, screenHeight});
  s_brewScreen->addWidget(new widget::ScaleStatus{5, 7, 80});
  s_brewScreen->addWidget(new widget::MachineStatus{screenWidth / 3 + 5, 7, 80});
  s_brewScreen->addWidget(new widget::ShotTimer{(screenWidth / 3) * 2 + 5, 7, 80});
  s_brewScreen->addWidget(new widget::ShotGraph{5, 40, screenWidth - 30, screenHeight - 57});
  s_brewScreen->addWidget(new widget::WaterLevel{screenWidth - 18, 42, 10, screenHeight - 49});
  s_brewScreen->addWidget(new widget::ProfileName{8, screenHeight - 14, screenWidth - 34});

  s_connectScreen = new Screen{ScreenID::connect};
  s_connectScreen->addWidget(new widget::ConnectInstructions{screenWidth, screenHeight});

  s_configScreen = new Screen{ScreenID::config};
  s_configScreen->addWidget(new widget::ConfigBackground{screenWidth, screenHeight});
  s_configScreen->addWidget(new widget::ConfigFields{18, 46, screenWidth - 36, screenHeight - 46});

  s_feedbackScreen = new Screen{ScreenID::feedback};
  s_feedbackScreen->addWidget(new widget::FeedbackBanner{screenWidth, screenHeight});

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
  tft.init();

  ledcSetup(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_PWM_FREQ, BACKLIGHT_PWM_RESOLUTION);
  ledcAttachPin(TFT_BL, BACKLIGHT_PWM_CHANNEL);
  ledcWrite(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_ON);
#endif

  // when flipped the physical positions of our buttons swap, keep the lower
  // button as the profile / dismiss button and the upper one as menu
  auto menuPin = MENU_BUTTON_PIN;
#ifdef PROFILE_BUTTON_PIN
  auto profilePin = PROFILE_BUTTON_PIN;
  if (g_ctx.config.isFlipped()) {
    menuPin = PROFILE_BUTTON_PIN;
    profilePin = MENU_BUTTON_PIN;
  }
  pinMode(profilePin, INPUT_PULLUP);
  attachInterrupt(profilePin, profileButtonPressed, FALLING);
#endif

  pinMode(menuPin, INPUT_PULLUP);
  attachInterrupt(menuPin, menuButtonPressed, FALLING);

  tft.setRotation(g_ctx.config.isFlipped() ? 1 : 3);
  tft.setSwapBytes(true);
  tft.fillScreen(theme.bg_color);
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

// how long a profile selection needs to settle before we persist and upload it
const unsigned long PROFILE_SETTLE_TICKS = 3000 / TICK_TARGET;

// our target shot time, the configurable margin decides when we call it out
const double TARGET_SHOT_SECONDS = 30;

void loop() {
  // container for the commands we pop off our queue
  auto d = data::DataUpdate{UpdateType::null_update};
  auto nextTick = 0;
  idleStart = millis();

  auto lastScreen = ScreenID::unknown;
  auto lastState = MachineState::unknown;
  auto lastSubstate = MachineSubstate::unknown;
  auto lastConfigVersion = g_ctx.config.getVersion();
  auto lastProfile = g_ctx.config.getProfile();
  unsigned long profileChangedTick = 0;
  unsigned long pourStart = 0;
  auto lastFeedback = FeedbackType::none;
  bool waterWarnDismissed = false;
  auto lastFlipped = g_ctx.config.isFlipped();

  while (true) {
    while (xQueueReceive(updateQ, (void*)&d, 0) == pdTRUE) {
      d.apply(&g_ctx);
    }

    // ready for our next tick?
    if (millis() > nextTick) {
      // paint all our screens
      s_brewScreen->tickAndPaint(g_ctx, tft);
      s_connectScreen->tickAndPaint(g_ctx, tft);
      s_configScreen->tickAndPaint(g_ctx, tft);
      s_feedbackScreen->tickAndPaint(g_ctx, tft);

      // we just went to sleep, turn off the screen
      if (g_ctx.machineState == MachineState::sleep && lastState != g_ctx.machineState) {
#ifdef M5_STICK
        M5.Axp.ScreenSwitch(false);
#endif        
#ifdef TTGO
        ledcWrite(BACKLIGHT_PWM_CHANNEL, 0);
        tft.writecommand(ST7789_DISPOFF);
#endif
#ifdef ESP32_2432S028R
  digitalWrite(TFT_BL, LOW);
#endif

        // send a sleep command to our BLE devices
        auto sleep = cmd::CommandRequest::newSleepCommand();
        xQueueSend(cmdQ, &sleep, 10);
      }

      // just left sleep, turn on screen
      if ((lastState == MachineState::sleep || lastState == MachineState::unknown) &&
          g_ctx.machineState != MachineState::sleep) {
#ifdef M5_STICK
        M5.Axp.ScreenSwitch(true);
#endif
#ifdef TTGO
        ledcWrite(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_ON);
        tft.writecommand(ST7789_DISPON);
#endif
        idleStart = millis();

        // send a wake command to our BLE devices
        auto wake = cmd::CommandRequest::newWakeCommand();
        xQueueSend(cmdQ, &wake, 10);
      }

      // starting a new shot clears any lingering feedback
      if (g_ctx.machineState == MachineState::espresso && lastState != MachineState::espresso) {
        g_ctx.feedback = FeedbackType::none;
        g_ctx.feedbackPreview = false;
        if (g_ctx.screen == ScreenID::feedback) {
          g_ctx.screen = ScreenID::brew;
        }
      }

      // track how long we pour so we can give grind feedback after the shot
      if (g_ctx.machineState == MachineState::espresso && g_ctx.machineSubstate == MachineSubstate::pouring &&
          lastSubstate != MachineSubstate::pouring) {
        pourStart = millis();
      }

      if (lastSubstate == MachineSubstate::pouring && g_ctx.machineSubstate != MachineSubstate::pouring &&
          pourStart > 0) {
        auto seconds = (millis() - pourStart) / (double)1000;
        g_ctx.feedbackSeconds = seconds;
        auto margin = (double)g_ctx.config.getShotMargin();
        if (fabs(seconds - TARGET_SHOT_SECONDS) <= 0.5) {
          g_ctx.feedback = FeedbackType::nailed_it;
        } else if (seconds <= TARGET_SHOT_SECONDS - margin) {
          g_ctx.feedback = FeedbackType::grind_finer;
        } else if (seconds >= TARGET_SHOT_SECONDS + margin) {
          g_ctx.feedback = FeedbackType::grind_coarser;
        } else {
          g_ctx.feedback = FeedbackType::none;
        }

        // take over the screen with our banner when we have feedback
        if (g_ctx.feedback != FeedbackType::none && g_ctx.screen == ScreenID::brew) {
          g_ctx.screen = ScreenID::feedback;
        }
        pourStart = 0;
      }

      // note when the add water banner is dismissed while still low so we don't
      // immediately show it again
      if (lastFeedback == FeedbackType::add_water && g_ctx.feedback == FeedbackType::none &&
          g_ctx.waterLevel <= g_ctx.config.getWarnLevel()) {
        waterWarnDismissed = true;
      }
      lastFeedback = g_ctx.feedback;

      // moving water re-arms the warning, as does refilling above the level
      if (machineMovingWater(g_ctx.machineState) || g_ctx.waterLevel > g_ctx.config.getWarnLevel()) {
        waterWarnDismissed = false;
      }

      // suggest adding water when we are at rest in the warning zone
      if (g_ctx.machineState == MachineState::idle && g_ctx.waterLevel > 0 &&
          g_ctx.waterLevel <= g_ctx.config.getWarnLevel() && !waterWarnDismissed &&
          g_ctx.feedback == FeedbackType::none && g_ctx.screen == ScreenID::brew) {
        g_ctx.feedback = FeedbackType::add_water;
        g_ctx.screen = ScreenID::feedback;
      }

      // if we just switched into brewing espresso, tare our scale
      if ((g_ctx.machineState != lastState && g_ctx.machineState == MachineState::espresso) ||
          (g_ctx.machineState == MachineState::espresso && lastSubstate < MachineSubstate::preinfusing &&
           g_ctx.machineSubstate >= MachineSubstate::preinfusing)) {
        if (g_ctx.tickID - lastTare > CMD_TIMEOUT) {
          auto tare = cmd::CommandRequest::newTareScaleCommand();
          xQueueSend(cmdQ, &tare, 10);
          lastTare = g_ctx.tickID;
        }
      }

      // if we are brewing and we are over 36grams, stop
      if (g_ctx.machineState == MachineState::espresso && g_ctx.machineSubstate == MachineSubstate::pouring &&
          g_ctx.currentWeight > g_ctx.config.getStopWeight() - 1) {
        if (g_ctx.tickID - lastStop > CMD_TIMEOUT) {
          auto stop = cmd::CommandRequest::newStopMachineCommand();
          xQueueSend(cmdQ, &stop, 10);
          lastStop = g_ctx.tickID;
        }
      }

      // switching into idle, reset our timeout
      if (g_ctx.machineState == MachineState::idle && lastState != MachineState::idle) {
        idleStart = millis();
      }

      // if we haven't brewed anything for a bit, sleep
      if (g_ctx.machineState == MachineState::idle && millis() - idleStart > g_ctx.config.getSleepTime() * 60 * 1000) {
        if (g_ctx.tickID - lastSleep > CMD_TIMEOUT) {
          auto sleep = cmd::CommandRequest::newSleepCommand();
          xQueueSend(cmdQ, &sleep, 10);
          lastSleep = g_ctx.tickID;
        }
      }

      // if our config changed, pass along our new refill level
      if (g_ctx.config.getVersion() != lastConfigVersion) {
        lastConfigVersion = g_ctx.config.getVersion();
        auto refill = cmd::CommandRequest::newRefillLevelCommand(g_ctx.config.getRefillLevel());
        xQueueSend(cmdQ, &refill, 10);
      }

      // if our selected profile changed, start (or restart) our settle timer, this
      // lets the user cycle through profiles without each one hitting the machine
      if (g_ctx.config.getProfile() != lastProfile) {
        lastProfile = g_ctx.config.getProfile();
        profileChangedTick = g_ctx.tickID;
      }

      // once the selection has settled, persist it and upload it to the machine
      if (profileChangedTick > 0 && g_ctx.tickID - profileChangedTick > PROFILE_SETTLE_TICKS &&
          g_ctx.machineState != MachineState::espresso && g_ctx.machineState != MachineState::steam &&
          g_ctx.machineState != MachineState::hot_water) {
        writeConfig(g_ctx.config);
        auto profile = cmd::CommandRequest::newProfileCommand(g_ctx.config.getProfile() - 1);
        xQueueSend(cmdQ, &profile, 10);
        profileChangedTick = 0;
      }

      // orientation changes need a restart to take effect
      if (g_ctx.config.isFlipped() != lastFlipped) {
        lastFlipped = g_ctx.config.isFlipped();
        g_ctx.restartTickID = g_ctx.tickID + restart_tick_delay;
      }

      // if it's time to reboot, do so
      if (g_ctx.restartTickID > 0 && g_ctx.tickID > g_ctx.restartTickID) {
        ESP.restart();
      }

      // handle our AP state based on what screen we are on, start and stop
      // are safe to call regardless of current AP state
      if (g_ctx.screen == ScreenID::connect || g_ctx.screen == ScreenID::config) {
        startAP();
      } else if (g_ctx.screen == ScreenID::brew) {
        stopAP();
      }

      lastScreen = g_ctx.screen;
      lastState = g_ctx.machineState;
      lastSubstate = g_ctx.machineSubstate;

      // increment our tick id and save our last tick
      g_ctx.tickID++;
      lastTick = millis();
      nextTick = lastTick + TICK_TARGET;

      if (g_ctx.screen == ScreenID::config || g_ctx.screen == ScreenID::connect) {
        g_dnsServer.processNextRequest();
      }
    }
  }
}