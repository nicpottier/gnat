#pragma once

#include <Data.h>
#include <ble/Device.h>

namespace ble {

const auto de1_name = "DE1";
const auto de1_requested_state_uuid = "0xa002";
const auto de1_state_uuid = "0xa00e";
const auto de1_water_uuid = "0xa011";
const auto de1_sample_uuid = "0xa00d";

const uint8_t de1_sleep_cmd = 0x00;
const uint8_t de1_stop_cmd = 0x02;

class DE1 : public Device, public Machine {
 public:
  DE1(QueueHandle_t updateQ, QueueHandle_t cmdQ)
      : Device(DeviceType::machine, updateQ, cmdQ) {}

  bool stop() {
    if (!m_cmdCharacteristic) {
      return false;
    }
    return m_cmdCharacteristic->writeValue(de1_stop_cmd);
  }

  bool sleep() {
    if (!m_cmdCharacteristic) {
      return false;
    }
    return m_cmdCharacteristic->writeValue(de1_sleep_cmd);
  }

  void stateUpdate(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* d, size_t length, bool isNotify) {
    int state = d[0];
    int subState = d[1];
    queueUpdate(data::DataUpdate::newMachineStateUpdate((MachineState)state, (MachineSubstate)subState));
  }

  void sampleUpdate(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* d, size_t length, bool isNotify) {
    if (length != 19) {
      return;
    }
    auto sample = data::Sample{};
    int i16 = d[1] | (d[0] << 8);
    sample.sampleTime = int(100 * (i16 / double(50 * 2)));

    i16 = d[3] | (d[2] << 8);
    sample.groupPressure = i16 / double(1 << 12);
    i16 = d[5] | (d[4] << 8);
    sample.groupFlow = i16 / double(1 << 12);
    i16 = d[7] | (d[6] << 8);
    sample.mixTemp = i16 / double(1 << 8);

    sample.headTemp = ((d[8] << 16) + (d[9] << 8) + d[10]) / double(1 << 16);
    i16 = d[12] | (d[11] << 8);
    sample.targetMixTemp = i16 / double(1 << 8);
    i16 = d[14] | (d[13] << 8);
    sample.targetHeadTemp = i16 / double(1 << 8);
    sample.targetGroupPressure = d[15] / double(1 << 4);
    sample.targetGroupFlow = d[16] / double(1 << 4);
    sample.frameNumber = d[17];
    sample.steamTemp = d[18];

    queueUpdate(data::DataUpdate::newSampleUpdate(sample));
  }

  void waterUpdate(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* d, size_t length, bool isNotify) {
    ushort level = d[1] | (d[2] << 8);
    ushort threshold = d[3] | (d[4] << 8);
    queueUpdate(data::DataUpdate::newWaterLevelUpdate(level, threshold));
  }

  bool setupConnection(NimBLEClient* c) {
    Serial.printf("[%s] client connection id %d\n", getName().c_str(), c->getConnId());
    c->setConnectTimeout(1);

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
        }

        if (ch->getUUID().toString() == de1_requested_state_uuid) {
          m_cmdCharacteristic = ch;
        }

        if (ch->canNotify()) {
          Serial.print("  CAN NOTIFY");
          // state update
          if (ch->getUUID().toString() == de1_state_uuid) {
            ch->subscribe(true, std::bind(&DE1::stateUpdate, this, std::placeholders::_1, std::placeholders::_2,
                                          std::placeholders::_3, std::placeholders::_4));
            Serial.print(" STATE");

            auto value = ch->readValue();
            int state = value.data()[0];
            int subState = value.data()[1];
            queueUpdate(data::DataUpdate::newMachineStateUpdate((MachineState)state, (MachineSubstate)subState));
          }
          // sample update
          if (ch->getUUID().toString() == de1_sample_uuid) {
            ch->subscribe(true, std::bind(&DE1::sampleUpdate, this, std::placeholders::_1, std::placeholders::_2,
                                          std::placeholders::_3, std::placeholders::_4));
            Serial.print(" SAMPLE");
          }
          // water update
          if (ch->getUUID().toString() == de1_water_uuid) {
            ch->subscribe(true, std::bind(&DE1::waterUpdate, this, std::placeholders::_1, std::placeholders::_2,
                                          std::placeholders::_3, std::placeholders::_4));
            Serial.print(" WATER");
          }
        }
        Serial.println("");
      }
    }

    return true;
  }

  void teardownConnection(NimBLEClient* c) {}

  void selfRegister(Devices* devices) {
    devices->setMachine(this);
  }

  bool shouldConnect(NimBLEAdvertisedDevice* d) {
    // name returned by BLE is null terminated (in a std::string!) so fallback to strcmp
    return (strcmp(de1_name, d->getName().c_str())) == 0;
  }
  const std::string getName() {
    return de1_name;
  }

 private:
  NimBLERemoteCharacteristic* m_cmdCharacteristic = nullptr;
};

}  // namespace ble