#pragma once

#include <ble/Device.h>

namespace ble {

const auto SKALE_NAME = std::string{"Skale"};
const auto SKALE_CMD_UUID = "0xef80";
const auto SKALE_WEIGHT_UUID = "0xef81";

const auto SKALE_TARE = 0x10;
const auto SKALE_DISPLAY_ON = 0xED;
const auto SKALE_DISPLAY_OFF = 0xEE;
const auto SKALE_GRAMS = 0x03;
const auto SKALE_DISPLAY_WEIGHT = 0xEC;

class Skale : public Device, public Scale {
 public:
  Skale(QueueHandle_t updateQ, QueueHandle_t cmdQ) : Device(DeviceType::SCALE, updateQ, cmdQ) {}

  void scaleUpdate(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (length > 4) {
      int16_t w = pData[1] | (pData[2] << 8);
      queueUpdate(data::DataUpdate::newWeightCommand(w / (double)10));
    }
  }

  bool tare() {
    Serial.println("TARE!?!");
    if (!m_cmdCharacteristic) {
      Serial.println("NO CHAR, NOT TARING");
      return false;
    }

    Serial.println("TARING SCALE");
    return m_cmdCharacteristic->writeValue(SKALE_TARE);
  }

  bool init() {
    if (!m_cmdCharacteristic) {
      Serial.println("NO CHAR, NOT INITING");
      return false;
    }

    return m_cmdCharacteristic->writeValue(SKALE_TARE) && m_cmdCharacteristic->writeValue(SKALE_DISPLAY_ON) &&
           m_cmdCharacteristic->writeValue(SKALE_DISPLAY_WEIGHT) && m_cmdCharacteristic->writeValue(SKALE_GRAMS);
  }

  void teardownConnection(NimBLEClient* c) { m_cmdCharacteristic = nullptr; }

  bool setupConnection(NimBLEClient* c) {
    c->setConnectTimeout(1);
    Serial.printf("[%s] client connection id %d\n", getName().c_str(), c->getConnId());

    std::vector<NimBLERemoteService*>* svcs = c->getServices(true);
    for (int i = 0; i < svcs->size(); i++) {
      NimBLERemoteService* svc = svcs->at(i);
      Serial.println("-------------------------------");
      Serial.print("Service: ");
      Serial.println(svc->getUUID().toString().c_str());

      std::vector<NimBLERemoteCharacteristic*>* cs = svc->getCharacteristics(true);
      for (int i = 0; i < cs->size(); i++) {
        NimBLERemoteCharacteristic* ch = cs->at(i);
        Serial.print("\t");
        Serial.print(ch->getUUID().toString().c_str());

        if (ch->canRead()) {
          Serial.print(" = ");
          Serial.print(ch->readValue().c_str());
        } else {
          Serial.print(" = WRITE ONLY");

          if (ch->getUUID().toString() == SKALE_CMD_UUID) {
            // characteristic is held by service, held by client, held by us, so should be safe
            m_cmdCharacteristic = ch;
          }
        }

        if (ch->canNotify()) {
          Serial.print("  CAN NOTIFY");
          if (ch->getUUID().toString() == SKALE_WEIGHT_UUID) {
            if (ch->subscribe(true,
                              std::bind(&Skale::scaleUpdate, this, std::placeholders::_1, std::placeholders::_2,
                                        std::placeholders::_3, std::placeholders::_4),
                              false)) {
              Serial.print(" SUBBED");
            } else {
              Serial.println("ERROR SUBBING TO SKALE");
              return false;
            }
          }
        }
        Serial.println("");
      }
    }

    // if we didn't find our command characteristic, fail
    if (!m_cmdCharacteristic) {
      return false;
    }

    // initialize the scale
    queueCommand(cmd::CommandRequest::newInitScaleCommand());
    return true;
  }

  bool shouldConnect(NimBLEAdvertisedDevice* d) { return (d->getName() == SKALE_NAME); }
  const std::string getName() { return SKALE_NAME; }

 private:
  NimBLERemoteCharacteristic* m_cmdCharacteristic = nullptr;
};
};  // namespace ble