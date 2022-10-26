#pragma once

#include <ble/Device.h>

namespace ble {

const auto skale_name = std::string{"Skale"};
const auto skale_cmd_uuid = "0xef80";
const auto skale_weight_uuid = "0xef81";

const auto skale_tare_cmd = 0x10;
const auto skale_display_on_cmd = 0xED;
const auto skale_display_off_cmd = 0xEE;
const auto skale_grams = 0x03;
const auto skale_display_weight = 0xEC;

class Skale : public Device, public Scale {
 public:
  Skale(QueueHandle_t updateQ, QueueHandle_t cmdQ)
      : Device(DeviceType::scale, updateQ, cmdQ) {}

  void scaleUpdate(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (length > 4) {
      int16_t w = pData[1] | (pData[2] << 8);
      queueUpdate(data::DataUpdate::newWeightCommand(w / (double)10));
    }
  }

  bool tare() override {
    if (!m_cmdChar) {
      return false;
    }

    return m_cmdChar->writeValue(skale_tare_cmd);
  }

  bool init() {
    if (!m_cmdChar) {
      return false;
    }

    return m_cmdChar->writeValue(skale_tare_cmd) && m_cmdChar->writeValue(skale_display_on_cmd) &&
           m_cmdChar->writeValue(skale_display_weight) && m_cmdChar->writeValue(skale_grams);
  }

  bool sleep() {
    if (!m_cmdChar) {
      return false;
    }

    return m_cmdChar->writeValue(skale_display_off_cmd);
  }

  bool wake() {
    if (!m_cmdChar) {
      return false;
    }

    return m_cmdChar->writeValue(skale_tare_cmd) && m_cmdChar->writeValue(skale_display_on_cmd) &&
           m_cmdChar->writeValue(skale_display_weight) && m_cmdChar->writeValue(skale_grams);
  }

  void teardownConnection(NimBLEClient* c) {
    m_cmdChar = nullptr;
  }

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

          if (ch->getUUID().toString() == skale_cmd_uuid) {
            // characteristic is held by service, held by client, held by us, so should be safe
            m_cmdChar = ch;
          }
        }

        if (ch->canNotify()) {
          Serial.print("  CAN NOTIFY");
          if (ch->getUUID().toString() == skale_weight_uuid) {
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
    if (!m_cmdChar) {
      return false;
    }

    // initialize the scale
    queueCommand(cmd::CommandRequest::newInitScaleCommand());
    return true;
  }

  void selfRegister(Devices* devices) {
    devices->setScale(this);
  }
  bool shouldConnect(NimBLEAdvertisedDevice* d) {
    return (d->getName() == skale_name);
  }
  const std::string getName() {
    return skale_name;
  }

 private:
  NimBLERemoteCharacteristic* m_cmdChar = nullptr;
};
};  // namespace ble