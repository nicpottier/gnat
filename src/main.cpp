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

#ifdef DISPLAY_RM67162
#include <amoled/rm67162.h>
#endif

#ifdef TOUCH_CST816
#include <touch/CST816.h>
CST816 g_touch;
bool g_touchWasDown = false;
bool g_touchCircle = false;
int g_touchStartX = 0;
int g_touchStartY = 0;
int g_touchLastX = 0;
int g_touchLastY = 0;
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
#include <widget/AdjustFields.h>
#include <widget/ConnectInstructions.h>
#include <widget/FeedbackBanner.h>
#include <widget/MachineStatus.h>
#include <widget/PourScreen.h>
#include <widget/ProfileName.h>
#include <widget/ScaleStatus.h>
#include <widget/ShotGraph.h>
#include <widget/ShotTimer.h>
#include <widget/Splash.h>
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

#ifdef DISPLAY_RM67162
// AMOLED panels render into a full screen sprite that we push over QSPI
TFT_eSprite g_frame = TFT_eSprite(&tft);
#endif

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
static Screen* s_adjustScreen;
static Screen* s_splashScreen;
static Screen* s_pourScreen;

const int BACKLIGHT_PWM_FREQ = 10000;
const int BACKLIGHT_PWM_RESOLUTION = 8;
const int BACKLIGHT_PWM_CHANNEL = 0;

#ifndef M5_STICK
// some backlight drivers (t-display-s3-pro) step dim on pulses, PWM turns them
// off, those boards define BACKLIGHT_DIGITAL and get simple on / off instead
void backlightOn() {
#ifdef BACKLIGHT_DIGITAL
  digitalWrite(TFT_BL, HIGH);
#else
  ledcWrite(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_ON);
#endif
}

void backlightOff() {
#ifdef BACKLIGHT_DIGITAL
  digitalWrite(TFT_BL, LOW);
#else
  ledcWrite(BACKLIGHT_PWM_CHANNEL, 0);
#endif
}
#endif

auto g_ctx = data::Context{};

// a shot log we serve at /shots so stops can be audited without a computer
struct ShotLogEntry {
  int seq;
  double target;
  double stopAt;
  double rate;
  double settled;
  int lagTenths;
};
const int SHOT_LOG_SIZE = 32;
ShotLogEntry g_shotLog[SHOT_LOG_SIZE];
int g_shotLogCount = 0;

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
    char query[600];
    config.toURLQuery(query, 600);

    char url[900];
    snprintf(url, 900, "/config?%s&msg=%s", query, msg);
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

    else if (strstr(url, "/shots") == url) {
      // the shot log: how each stop was decided and where the shot landed,
      // newest first, so stop lag tuning can be audited from a phone
      auto res = request->beginResponseStream("text/html");
      res->print(
          "<!doctype html><html><head><meta name=viewport content='width=device-width, initial-scale=1'>"
          "<title>GNAT Shots</title></head><body style='font-family:sans-serif'>");
      res->printf("<h3>Shots &mdash; stop lag %0.1fs (%d samples)</h3>", g_ctx.config.stopLagSeconds(),
                  g_ctx.config.getLagSamples());
      res->print(
          "<table border=1 cellpadding=6 cellspacing=0>"
          "<tr><th>#</th><th>Target g</th><th>Stopped at g</th><th>Rate g/s</th><th>Settled g</th>"
          "<th>Lag used s</th></tr>");
      auto count = min(g_shotLogCount, SHOT_LOG_SIZE);
      for (int i = 0; i < count; i++) {
        auto& e = g_shotLog[(g_shotLogCount - 1 - i) % SHOT_LOG_SIZE];
        res->printf("<tr><td>%d</td><td>%0.0f</td><td>%0.1f</td><td>%0.2f</td><td>%0.1f</td><td>%0.1f</td></tr>",
                    e.seq, e.target, e.stopAt, e.rate, e.settled, e.lagTenths / 10.0);
      }
      res->print("</table><p>Log resets on reboot.</p></body></html>");
      request->send(res);
    }

    else if (strstr(url, "/profiles.json") == url) {
      auto response = request->beginResponse_P(200, "application/json", profiles_json);
      response->addHeader("Cache-Control", "no-store");
      request->send(response);
    }

    else if (strstr(url, "/config") == url) {
      if (request->method() == HTTP_POST) {
        auto newConfig = Config::fromRequest(request);

        // the web form doesn't manage the profile or learned stop lag,
        // carry ours over
        newConfig.setProfile(g_ctx.config.getProfile());
        newConfig.setStopLag(g_ctx.config.getStopLag());
        newConfig.setLagSamples(g_ctx.config.getLagSamples());

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

#ifdef COMBO_BUTTON_PIN
// single button boards: a short press cycles profiles / dismisses feedback,
// a long press cycles screens like the menu button
volatile unsigned long comboPressStart = 0;

void IRAM_ATTR comboButtonChanged() {
  bool pressed = digitalRead(COMBO_BUTTON_PIN) == LOW;
  auto now = millis();

  if (pressed) {
    comboPressStart = now;
    return;
  }

  if (comboPressStart == 0) {
    return;
  }
  auto held = now - comboPressStart;
  comboPressStart = 0;

  // too short to be a real press
  if (held < 50) {
    return;
  }

  if (held >= 600) {
    auto nextScreen = ScreenID::brew;
    if (g_ctx.screen == ScreenID::brew) {
      nextScreen = ScreenID::connect;
    } else if (g_ctx.screen == ScreenID::connect) {
      nextScreen = ScreenID::config;
    }
    auto screen = data::DataUpdate::newScreenUpdate(nextScreen);
    xQueueSendFromISR(updateQ, &screen, NULL);
  } else {
    auto cycle = data::DataUpdate::newProfileCycleUpdate();
    xQueueSendFromISR(updateQ, &cycle, NULL);
  }
}
#endif

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

#ifdef BOARD_POWER_PIN
  // power cycle the display rail, warm resets can leave the panel latched in
  // a dark state that only losing power clears
  pinMode(BOARD_POWER_PIN, OUTPUT);
  digitalWrite(BOARD_POWER_PIN, LOW);
  delay(200);
  digitalWrite(BOARD_POWER_PIN, HIGH);
  delay(100);
#endif

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
  de1->setFlushSeconds(g_ctx.config.getFlushSeconds());

  s_brewScreen = new Screen{ScreenID::brew};
  s_brewScreen->addWidget(new widget::BrewBackground{screenWidth, screenHeight});
  s_brewScreen->addWidget(new widget::ScaleStatus{px(5), px(4), px(80)});
  s_brewScreen->addWidget(new widget::MachineStatus{screenWidth / 3 + px(5), px(4), px(80)});
  s_brewScreen->addWidget(new widget::ShotTimer{(screenWidth / 3) * 2 + px(5), px(4), px(70)});
  s_brewScreen->addWidget(new widget::ShotGraph{px(5), px(28), screenWidth - px(30), screenHeight - px(45)});
  s_brewScreen->addWidget(new widget::WaterLevel{screenWidth - px(18), px(3), px(10), screenHeight - px(6)});
  s_brewScreen->addWidget(new widget::ProfileName{px(8), screenHeight - px(14), screenWidth - px(34)});

  s_connectScreen = new Screen{ScreenID::connect};
  s_connectScreen->addWidget(new widget::ConnectInstructions{screenWidth, screenHeight});

  s_configScreen = new Screen{ScreenID::config};
  s_configScreen->addWidget(new widget::ConfigBackground{screenWidth, screenHeight});
  s_configScreen->addWidget(new widget::ConfigFields{px(18), px(46), screenWidth - px(36), screenHeight - px(46)});

  s_feedbackScreen = new Screen{ScreenID::feedback};
  s_feedbackScreen->addWidget(new widget::FeedbackBanner{screenWidth, screenHeight});

  s_adjustScreen = new Screen{ScreenID::adjust};
  s_adjustScreen->addWidget(new widget::AdjustFields{screenWidth, screenHeight});

  s_splashScreen = new Screen{ScreenID::splash};
  s_splashScreen->addWidget(new widget::Splash{screenWidth, screenHeight});

  s_pourScreen = new Screen{ScreenID::pour};
  s_pourScreen->addWidget(new widget::PourScreen{screenWidth, screenHeight});

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
#elif defined(DISPLAY_RM67162)
  rm67162_init();
  lcd_setRotation(g_ctx.config.isFlipped() ? 1 : 3);
  g_frame.createSprite(screenWidth, screenHeight);
  g_frame.setSwapBytes(true);
  g_frame.fillSprite(theme.bg_color);

#ifdef TOUCH_CST816
  if (g_touch.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN)) {
    Serial.println("[TOUCH] controller found");
  } else {
    Serial.println("[TOUCH] controller NOT found");
  }
#endif
#else
  tft.init();

#ifdef BACKLIGHT_DIGITAL
  pinMode(TFT_BL, OUTPUT);
#else
  ledcSetup(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_PWM_FREQ, BACKLIGHT_PWM_RESOLUTION);
  ledcAttachPin(TFT_BL, BACKLIGHT_PWM_CHANNEL);
#endif
  backlightOn();
#endif

#ifdef COMBO_BUTTON_PIN
  // single button, short press for profiles, long press for menu
  pinMode(COMBO_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(COMBO_BUTTON_PIN, comboButtonChanged, CHANGE);
#else
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
#endif

#ifndef DISPLAY_RM67162
  tft.setRotation(g_ctx.config.isFlipped() ? 1 : 3);
  tft.setSwapBytes(true);
  tft.fillScreen(theme.bg_color);
#endif
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

// the stop lag (how far the predictive stop leads the scale) lives in config
// and tunes itself from a rolling window of settled shots: the window median
// feeds an update whose gain decays with the persisted sample count, so early
// shots move (and write) the lag freely while a settled lag rarely changes
const int LAG_WINDOW = 8;

// the gain never decays below 1/this, so the lag keeps adapting slowly
const int LAG_GAIN_FLOOR = 16;

// how long after a pour ends before we read the settled weight
const unsigned long SETTLE_DELAY_TICKS = 5000 / TICK_TARGET;


// how long in millis the splash screen shows on boot and wake, long enough
// for the pour animation plus a beat of hold
const unsigned long SPLASH_TIME = 6300;

void loop() {
  // container for the commands we pop off our queue
  auto d = data::DataUpdate{UpdateType::null_update};
  auto nextTick = 0;
  idleStart = millis();

  // greet the user with our splash on boot
  g_ctx.screen = ScreenID::splash;
  unsigned long splashStart = millis();

  auto lastScreen = ScreenID::unknown;
  auto lastState = MachineState::unknown;
  auto lastSubstate = MachineSubstate::unknown;
  auto lastConfigVersion = g_ctx.config.getVersion();
  auto lastProfile = g_ctx.config.getProfile();
  unsigned long profileChangedTick = 0;
  unsigned long pourStart = 0;
  unsigned long adjustDirtyTick = 0;
  auto lastFeedback = FeedbackType::none;
  bool waterWarnDismissed = false;
  auto lastFlipped = g_ctx.config.isFlipped();

  // smoothed scale weight rate in g/s while a shot runs, for predictive stop
  double weightRate = 0;
  double weightRateWeight = 0;
  unsigned long weightRateMs = 0;

  // details of the stop we issued this pour, for lag tuning and the shot log
  bool stopIssued = false;
  double stopAtWeight = 0;
  double stopAtRate = 0;
  int stopLagUsed = 0;

  // pending settled weight measurement and the rolling window of implied
  // lags, the persisted sample count drives the adaptation gain down
  unsigned long settleTick = 0;
  double settleTarget = 0;
  double lagWindow[LAG_WINDOW];
  int lagWindowCount = 0;
  int lagSampleCount = g_ctx.config.getLagSamples();

  while (true) {
    while (xQueueReceive(updateQ, (void*)&d, 0) == pdTRUE) {
      d.apply(&g_ctx);
    }

    // ready for our next tick?
    if (millis() > nextTick) {
      // paint all our screens
#ifdef DISPLAY_RM67162
      TFT_eSPI& gfx = g_frame;
#else
      TFT_eSPI& gfx = tft;
#endif
      bool painted = s_brewScreen->tickAndPaint(g_ctx, gfx);
      painted |= s_connectScreen->tickAndPaint(g_ctx, gfx);
      painted |= s_configScreen->tickAndPaint(g_ctx, gfx);
      painted |= s_feedbackScreen->tickAndPaint(g_ctx, gfx);
      painted |= s_adjustScreen->tickAndPaint(g_ctx, gfx);
      painted |= s_splashScreen->tickAndPaint(g_ctx, gfx);
      painted |= s_pourScreen->tickAndPaint(g_ctx, gfx);

#ifdef DISPLAY_RM67162
      // push our frame to the panel when anything changed
      if (painted) {
        lcd_PushColors(0, 0, screenWidth, screenHeight, (uint16_t*)g_frame.getPointer());
      }
#endif

      // we just went to sleep, turn off the screen
      if (g_ctx.machineState == MachineState::sleep && lastState != g_ctx.machineState) {
#ifdef M5_STICK
        M5.Axp.ScreenSwitch(false);
#endif
#ifdef TTGO
        backlightOff();
        tft.writecommand(ST7789_DISPOFF);
#endif
#ifdef ESP32_2432S028R
        digitalWrite(TFT_BL, LOW);
#endif
#ifdef DISPLAY_RM67162
        lcd_sleep();
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
        backlightOn();
        tft.writecommand(ST7789_DISPON);
#endif
#ifdef ESP32_2432S028R
        digitalWrite(TFT_BL, HIGH);
#endif
#ifdef DISPLAY_RM67162
        // wake the panel and restore our frame, but only when we actually
        // slept, this branch also fires on boot with lastState still unknown
        if (lastState == MachineState::sleep) {
          lcd_wake();
          lcd_PushColors(0, 0, screenWidth, screenHeight, (uint16_t*)g_frame.getPointer());
        }
#endif
        idleStart = millis();

        // waking from real sleep greets the user with our splash, boot rides
        // through this branch too but already showed it
        if (lastState == MachineState::sleep) {
          g_ctx.screen = ScreenID::splash;
          splashStart = millis();
        }

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

      // without a scale we can't stop at weight, refuse to run the shot,
      // stopping the machine and telling the user why
      if (g_ctx.config.requireScale() && g_ctx.machineState == MachineState::espresso &&
          g_ctx.getScaleBLEState() != BLEState::connected) {
        if (g_ctx.tickID - lastStop > CMD_TIMEOUT) {
          auto stop = cmd::CommandRequest::newStopMachineCommand();
          xQueueSend(cmdQ, &stop, 10);
          lastStop = g_ctx.tickID;

          g_ctx.feedback = FeedbackType::no_scale;
          if (g_ctx.screen == ScreenID::brew || g_ctx.screen == ScreenID::pour) {
            g_ctx.screen = ScreenID::feedback;
          }
        }
      }

      // track how long we pour so we can give grind feedback after the shot
      if (g_ctx.machineState == MachineState::espresso && g_ctx.machineSubstate == MachineSubstate::pouring &&
          lastSubstate != MachineSubstate::pouring) {
        pourStart = millis();
      }

      if (lastSubstate == MachineSubstate::pouring && g_ctx.machineSubstate != MachineSubstate::pouring &&
          pourStart > 0) {
        // log where the shot landed against the target for stop lag tuning
        Serial.printf("[SHOT] final weight %0.1f target %d\n", g_ctx.currentWeight, g_ctx.config.getStopWeight());

        // if we stopped this pour, come back for the settled weight once the
        // drip finishes
        if (stopIssued) {
          settleTick = g_ctx.tickID;
          settleTarget = double(g_ctx.config.getStopWeight());
        }

        auto seconds = (millis() - pourStart) / (double)1000;
        g_ctx.feedbackSeconds = seconds;
        auto target = (double)g_ctx.config.getShotTarget();
        auto margin = (double)g_ctx.config.getShotMargin();
        if (fabs(seconds - target) <= 0.5) {
          g_ctx.feedback = FeedbackType::nailed_it;
        } else if (seconds <= target - margin) {
          g_ctx.feedback = FeedbackType::grind_finer;
        } else if (seconds >= target + margin) {
          g_ctx.feedback = FeedbackType::grind_coarser;
        } else {
          g_ctx.feedback = FeedbackType::none;
        }

        // take over the screen with our banner when we have feedback
        if (g_ctx.feedback != FeedbackType::none &&
            (g_ctx.screen == ScreenID::brew || g_ctx.screen == ScreenID::pour)) {
          g_ctx.screen = ScreenID::feedback;
        }
        pourStart = 0;
      }

      // the add water banner dismisses itself once the tank is topped up
      if (g_ctx.feedback == FeedbackType::add_water && !g_ctx.feedbackPreview &&
          g_ctx.waterLevel > g_ctx.config.getWarnLevel()) {
        g_ctx.feedback = FeedbackType::none;
        if (g_ctx.screen == ScreenID::feedback) {
          g_ctx.screen = ScreenID::brew;
        }
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
          g_ctx.feedback == FeedbackType::none &&
          (g_ctx.screen == ScreenID::brew || g_ctx.screen == ScreenID::pour)) {
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

      // track how fast the cup is filling, smoothed, so we can stop early
      // enough for the shot to land on target
      if (g_ctx.machineState != MachineState::espresso) {
        weightRate = 0;
        weightRateMs = 0;
      } else if (millis() - weightRateMs >= 100) {
        if (weightRateMs > 0) {
          auto dt = (millis() - weightRateMs) / 1000.0;
          auto inst = (g_ctx.currentWeight - weightRateWeight) / dt;
          weightRate = weightRate * 0.7 + inst * 0.3;
        }
        weightRateMs = millis();
        weightRateWeight = g_ctx.currentWeight;
      }

      // a fresh pour clears the last stop record and any pending settle
      if (g_ctx.machineSubstate == MachineSubstate::pouring && lastSubstate != MachineSubstate::pouring) {
        stopIssued = false;
        settleTick = 0;
      }

      // if we are brewing, stop when the projected weight reaches the target,
      // the projection leads the actual weight by our learned stop lag so the
      // drip lands the shot on the number instead of past it
      if (g_ctx.machineState == MachineState::espresso && g_ctx.machineSubstate == MachineSubstate::pouring) {
        auto target = double(g_ctx.config.getStopWeight());
        auto projected = g_ctx.currentWeight + fmax(0.0, weightRate) * g_ctx.config.stopLagSeconds();
        if ((projected >= target || g_ctx.currentWeight > target - 1) && g_ctx.tickID - lastStop > CMD_TIMEOUT) {
          Serial.printf("[STOP] weight %0.1f rate %0.2f projected %0.1f target %0.0f lag %0.1f\n", g_ctx.currentWeight,
                        weightRate, projected, target, g_ctx.config.stopLagSeconds());
          auto stop = cmd::CommandRequest::newStopMachineCommand();
          xQueueSend(cmdQ, &stop, 10);
          lastStop = g_ctx.tickID;

          if (!stopIssued) {
            stopIssued = true;
            stopAtWeight = g_ctx.currentWeight;
            stopAtRate = fmax(0.0, weightRate);
            stopLagUsed = g_ctx.config.getStopLag();
          }
        }
      }

      // once the settle delay passes, grade the shot: log it, and when it
      // looks clean fold its implied lag into the tuning batch
      if (settleTick > 0 && g_ctx.tickID - settleTick >= SETTLE_DELAY_TICKS) {
        settleTick = 0;
        auto settled = g_ctx.currentWeight;

        auto& entry = g_shotLog[g_shotLogCount % SHOT_LOG_SIZE];
        entry.seq = g_shotLogCount + 1;
        entry.target = settleTarget;
        entry.stopAt = stopAtWeight;
        entry.rate = stopAtRate;
        entry.settled = settled;
        entry.lagTenths = stopLagUsed;
        g_shotLogCount++;

        // reject shots where the cup moved, the scale dropped or the pour
        // was barely flowing, they'd swing the lag wildly
        if (g_ctx.getScaleBLEState() == BLEState::connected && stopAtRate > 0.3 &&
            fabs(settled - settleTarget) < 5.0) {
          auto implied = stopLagUsed / 10.0 + (settled - settleTarget) / stopAtRate;
          implied = fmin(fmax(implied, min_stop_lag / 10.0), max_stop_lag / 10.0);
          lagWindow[lagWindowCount % LAG_WINDOW] = implied;
          lagWindowCount++;

          // the window median is what we chase, robust against the outliers
          // a single strange shot leaves behind
          int n = min(lagWindowCount, LAG_WINDOW);
          double sorted[LAG_WINDOW];
          for (int i = 0; i < n; i++) {
            sorted[i] = lagWindow[i];
            for (int j = i; j > 0 && sorted[j] < sorted[j - 1]; j--) {
              auto t = sorted[j];
              sorted[j] = sorted[j - 1];
              sorted[j - 1] = t;
            }
          }
          auto median = (n % 2) ? sorted[n / 2] : (sorted[n / 2 - 1] + sorted[n / 2]) / 2;

          // early samples move the lag fully, the gain decays with the
          // persisted sample count so a settled lag barely moves
          auto gain = 1.0 / min(lagSampleCount + 1, LAG_GAIN_FLOOR);
          auto lag = g_ctx.config.stopLagSeconds() + gain * (median - g_ctx.config.stopLagSeconds());
          lagSampleCount = min(lagSampleCount + 1, max_lag_samples);

          auto tenths = int(round(lag * 10));
          Serial.printf("[LAG] sample %d implied %0.2f median %0.2f gain %0.2f lag %0.1fs\n", lagSampleCount, implied,
                        median, gain, tenths / 10.0);

          // only hit flash when the rounded lag actually changes, frequent
          // early on and then rarely as the value settles
          if (tenths != g_ctx.config.getStopLag()) {
            g_ctx.config.setStopLag(tenths);
            g_ctx.config.setLagSamples(lagSampleCount);
            writeConfig(g_ctx.config);
            Serial.printf("[LAG] persisted %0.1fs after %d samples\n", g_ctx.config.stopLagSeconds(), lagSampleCount);
          }
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

#ifdef TOUCH_CST816
      // poll the touch controller, tracking presses so we can tell taps from
      // swipes when the finger lifts
      int touchX, touchY;
      bool touchDown = g_touch.read(touchX, touchY);

      if (touchDown) {
        // the round capacitive button reports out past the panel
        bool circle = touchX >= 560;

        // map the controller's coordinates into our ui space
        int ux = screenWidth - 1 - touchX;
        int uy = screenHeight - 1 - touchY;
        if (g_ctx.config.isFlipped()) {
          ux = screenWidth - 1 - ux;
          uy = screenHeight - 1 - uy;
        }

        if (!g_touchWasDown) {
          g_touchCircle = circle;
          g_touchStartX = ux;
          g_touchStartY = uy;

          // the circle is a home button, from any other screen it returns to
          // brew (dismissing any feedback), from the brew screen it opens the
          // menu screens
          if (circle) {
            g_ctx.feedback = FeedbackType::none;
            g_ctx.feedbackPreview = false;
            auto nextScreen = ScreenID::brew;
            if (g_ctx.screen == ScreenID::brew) {
              nextScreen = ScreenID::connect;
            }
            auto screen = data::DataUpdate::newScreenUpdate(nextScreen);
            xQueueSend(updateQ, &screen, 10);
          }
        }
        if (!circle) {
          g_touchLastX = ux;
          g_touchLastY = uy;
        }
      } else if (g_touchWasDown && !g_touchCircle) {
        // finger lifted, decide what the gesture was
        int dx = g_touchLastX - g_touchStartX;
        int dy = g_touchLastY - g_touchStartY;
        Serial.printf("[TOUCH] release at %d,%d delta %d,%d\n", g_touchLastX, g_touchLastY, dx, dy);

        if (abs(dx) >= px(50) && abs(dx) > abs(dy)) {
          // horizontal swipe, left moves forward through the adjust pages,
          // right moves back, wrapping to the brew screen at either end
          if (dx < 0) {
            if (g_ctx.screen == ScreenID::brew || g_ctx.screen == ScreenID::pour) {
              g_ctx.adjustPage = 0;
              g_ctx.screen = ScreenID::adjust;
            } else if (g_ctx.screen == ScreenID::adjust) {
              if (++g_ctx.adjustPage >= widget::adjust_page_count) {
                g_ctx.screen = ScreenID::brew;
              }
            }
          } else {
            if (g_ctx.screen == ScreenID::brew || g_ctx.screen == ScreenID::pour) {
              g_ctx.adjustPage = widget::adjust_page_count - 1;
              g_ctx.screen = ScreenID::adjust;
            } else if (g_ctx.screen == ScreenID::adjust) {
              if (g_ctx.adjustPage == 0) {
                g_ctx.screen = ScreenID::brew;
              } else {
                g_ctx.adjustPage--;
              }
            }
          }
        } else if (abs(dx) < px(15) && abs(dy) < px(15)) {
          // a tap toggles between the two brew screens
          if (g_ctx.screen == ScreenID::brew) {
            g_ctx.screen = ScreenID::pour;
          } else if (g_ctx.screen == ScreenID::pour) {
            g_ctx.screen = ScreenID::brew;
          } else if (g_ctx.screen == ScreenID::adjust && g_ctx.adjustPage < widget::adjust_page_count) {
            auto& page = widget::adjust_pages[g_ctx.adjustPage];

            // the profile page cycles through the enabled profiles
            if (!page.values) {
              for (int next = 0; next <= 1; next++) {
                int cx, cy;
                widget::adjustProfileButtonCenter(screenWidth, next, cx, cy);
                auto r = px(widget::adjust_button_r) * 3 / 2;
                if (abs(g_touchStartX - cx) <= r && abs(g_touchStartY - cy) <= r) {
                  g_ctx.config.setProfile(next ? g_ctx.config.nextEnabledProfile()
                                               : g_ctx.config.prevEnabledProfile());
                  adjustDirtyTick = g_ctx.tickID;
                }
              }
            }

            // tapping a +/- button steps its value
            for (int i = 0; i < page.valueCount; i++) {
              auto& value = page.values[i];

              if (value.names) {
                // named values have one wide toggle button
                int bx, by, bw, bh;
                widget::adjustToggleRect(page, screenWidth, i, bx, by, bw, bh);
                if (g_touchStartX >= bx && g_touchStartX <= bx + bw && g_touchStartY >= by - px(8) &&
                    g_touchStartY <= by + bh + px(8)) {
                  widget::adjustToggle(g_ctx.config, value);
                  adjustDirtyTick = g_ctx.tickID;
                }
                continue;
              }

              for (int plus = 0; plus <= 1; plus++) {
                int cx, cy;
                widget::adjustButtonCenter(page, screenWidth, i, plus, cx, cy);
                auto r = px(widget::adjust_button_r) * 3 / 2;
                if (abs(g_touchStartX - cx) <= r && abs(g_touchStartY - cy) <= r) {
                  value.set(g_ctx.config, value.get(g_ctx.config) + (plus ? value.step : -value.step));
                  adjustDirtyTick = g_ctx.tickID;
                }
              }
            }
          }
        }
      }
      g_touchWasDown = touchDown;

      // persist adjusted settings once the taps settle
      if (adjustDirtyTick > 0 && g_ctx.tickID - adjustDirtyTick > PROFILE_SETTLE_TICKS) {
        writeConfig(g_ctx.config);
        adjustDirtyTick = 0;
      }
#endif

      // if our config changed, pass along our new refill level and flush time
      if (g_ctx.config.getVersion() != lastConfigVersion) {
        lastConfigVersion = g_ctx.config.getVersion();
        auto refill = cmd::CommandRequest::newRefillLevelCommand(g_ctx.config.getRefillLevel());
        xQueueSend(cmdQ, &refill, 10);
        auto flush = cmd::CommandRequest::newFlushSecondsCommand(g_ctx.config.getFlushSeconds());
        xQueueSend(cmdQ, &flush, 10);
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

      // orientation changes need a restart to take effect, persist first so
      // the change survives the reboot
      if (g_ctx.config.isFlipped() != lastFlipped) {
        lastFlipped = g_ctx.config.isFlipped();
        writeConfig(g_ctx.config);
        g_ctx.restartTickID = g_ctx.tickID + restart_tick_delay;
      }

      // if it's time to reboot, do so
      if (g_ctx.restartTickID > 0 && g_ctx.tickID > g_ctx.restartTickID) {
        ESP.restart();
      }

      // the splash dismisses itself after a few seconds, unless the user
      // already moved on with a button or touch
      if (g_ctx.screen == ScreenID::splash && millis() - splashStart > SPLASH_TIME) {
        g_ctx.screen = ScreenID::brew;
      }

      // handle our AP state based on what screen we are on, start and stop
      // are safe to call regardless of current AP state
      if (g_ctx.screen == ScreenID::connect || g_ctx.screen == ScreenID::config) {
        startAP();
      } else if (g_ctx.screen == ScreenID::brew || g_ctx.screen == ScreenID::pour) {
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