#include <Arduino.h>
#include <Command.h>
#include <Data.h>
#include <M5StickCPlus.h>
#include <NimBLEDevice.h>
#include <ble/DE1.h>
#include <ble/FoundDevice.h>
#include <ble/Skale.h>
#include <widget/MachineStatus.h>
#include <widget/ScaleStatus.h>
#include <widget/ShotGraph.h>

// The build version comes from an environment variable. Use the VERSION
// define wherever the version is needed.
#define STRINGIZER(arg) #arg
#define STR_VALUE(arg) STRINGIZER(arg)
#define VERSION STR_VALUE(BUILD_VERSION)

static NimBLEScan* pBLEScan;
static ble::Device* devices[2];
static widget::Widget* widgets[3];

static QueueHandle_t foundDeviceQ;

static QueueHandle_t updateQ;
static QueueHandle_t cmdQ;

const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;

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
  auto devices = new cmd::Devices{};

  while (1) {
    // do we have anything to connect to?
    auto lastDeviceAddress = NimBLEAddress{};
    while (xQueueReceive(foundDeviceQ, (void*)&d, 0) == pdTRUE) {
      // collapse repeats
      if (d.getAddress() == lastDeviceAddress) {
        continue;
      }
      lastDeviceAddress = d.getAddress();

      auto device = d.getDevice();

      // if our device is already connected, move on
      if (!device->isDisconnected()) {
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

      // connect, save our device type if we succeed
      if (device->connect(client)) {
        switch (device->getType()) {
          case DeviceType::MACHINE:
            // devices->setMachine(reinterpret_cast<ble::Machine*>(device));
            devices->setMachine(de1);
            break;
          case DeviceType::SCALE:
            // devices->setScale(reinterpret_cast<ble::Scale*>(device));
            devices->setScale(skale);
            break;
          default:
            Serial.printf("UNKNOWN DEVICE TYPE: %d\n", device->getType());
        }
      } else {
        Serial.printf("[BLE] Failed to connect client for %s\n", d.getDevice()->getName().c_str());

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
    if (pBLEScan->isScanning() == false) {
      pBLEScan->start(0, nullptr, false);
    }

    // sleep a sec
    vTaskDelay(40 / portTICK_PERIOD_MS);
  }
}

void setup() {
  M5.begin();

  Serial.begin(115200);
  Serial.printf("[%d] Setup - Version: %s\n", xPortGetCoreID(), VERSION);

  foundDeviceQ = xQueueCreate(10, sizeof(ble::FoundDevice));
  updateQ = xQueueCreate(100, sizeof(data::DataUpdate));
  cmdQ = xQueueCreate(100, sizeof(cmd::CommandRequest));

  skale = new ble::Skale{updateQ, cmdQ};
  devices[0] = skale;
  de1 = new ble::DE1{updateQ, cmdQ};
  devices[1] = de1;

  widgets[0] = new widget::ScaleStatus{5, 7};
  widgets[1] = new widget::MachineStatus{120, 7};
  widgets[2] = new widget::ShotGraph{0, 35};

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

  // ledcSetup(pwmLedChannelTFT, pwmFreq, pwmResolution);
  // ledcAttachPin(TFT_BL, pwmLedChannelTFT);
  // ledcWrite(pwmLedChannelTFT, 100);

  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(COLOR_BG);

  M5.Lcd.fillRect(0, 0, 240, 35, COLOR_DASH_BG);
}

unsigned long tickID = 0;
unsigned long lastTick = 0;
unsigned long idleStart = 0;

auto ctx = data::Context{};

// how often we are trying to paint in millis, ~50 fps
const unsigned long TICK_TARGET = 20;

// how long in millis before we put the machine to sleep
const unsigned long SLEEP_TIMEOUT = 1000 * 60 * 15;

void loop() {
  // container for the commands we pop off our queue
  auto d = data::DataUpdate{UpdateType::EMPTY_UPDATE};
  auto nextTick = 0;
  idleStart = millis();

  auto lastState = MachineState::unknown;
  auto lastSubstate = 0;

  while (true) {
    while (xQueueReceive(updateQ, (void*)&d, 0) == pdTRUE) {
      d.apply(&ctx);
    }

    // ready for our next tick?
    if (millis() > nextTick) {
      // now tick and paint all our widgets
      for (auto widget : widgets) {
        auto repaint = widget->tick(ctx, tickID, millis());
        if (repaint) {
          widget->paint(M5.Lcd);
        }
      }

      // we just went to sleep, turn off the screen
      if (ctx.machineState == MachineState::sleep && lastState != ctx.machineState) {
        M5.Axp.ScreenSwitch(false);
      }

      // just left sleep, turn on screen
      if ((lastState == MachineState::sleep || lastState == MachineState::unknown) &&
          ctx.machineState != MachineState::sleep) {
        M5.Axp.ScreenSwitch(true);
        idleStart = millis();
      }

      // if we just switched into brewing espresso, tare our scale
      if ((ctx.machineState != lastState && ctx.machineState == MachineState::espresso) ||
          (ctx.machineState == MachineState::espresso && lastSubstate < 4 && ctx.machineSubstate >= 4)) {
        auto tare = cmd::CommandRequest::newTareScaleCommand();
        xQueueSend(cmdQ, &tare, 10);
      }

      // if we are brewing and we are over 36grams, stop
      if (ctx.machineState == MachineState::espresso && ctx.machineSubstate == 5 && ctx.currentWeight > 35) {
        auto stop = cmd::CommandRequest::newStopMachineCommand();
        xQueueSend(cmdQ, &stop, 10);
      }

      // switching into idle, reset our timeout
      if (ctx.machineState == MachineState::idle && lastState != MachineState::idle) {
        idleStart = millis();
      }

      // if we haven't brewed anything for a bit, sleep
      if (ctx.machineState == MachineState::idle && millis() - idleStart > SLEEP_TIMEOUT) {
        auto sleep = cmd::CommandRequest::newSleepMachineCommand();
        xQueueSend(cmdQ, &sleep, 10);
      }

      lastState = ctx.machineState;
      lastSubstate = ctx.machineSubstate;

      // increment our tick id and save our last tick
      tickID++;
      lastTick = millis();
      nextTick = lastTick + TICK_TARGET;
    }
  }
}
