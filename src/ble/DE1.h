#pragma once

#include <Data.h>
#include <ble/Device.h>
#include <profiles.h>

namespace ble {

const auto de1_name = "DE1";
const auto de1_requested_state_uuid = "0xa002";
const auto de1_state_uuid = "0xa00e";
const auto de1_water_uuid = "0xa011";
const auto de1_sample_uuid = "0xa00d";
const auto de1_header_uuid = "0xa00f";
const auto de1_frame_uuid = "0xa010";
const auto de1_mmr_write_uuid = "0xa006";
const auto de1_shot_settings_uuid = "0xa00b";

// MMR register holding the flush timeout, value is tenths of a second
const uint32_t de1_mmr_flush_timeout = 0x803848;

const uint8_t de1_sleep_cmd = 0x00;
const uint8_t de1_stop_cmd = 0x02;

// water level (in mm) at which the machine should ask for a refill, used until the
// configured value is set, without this write the machine falls back to a higher
// stored threshold and asks for water sooner than it needs to
const uint8_t de1_default_refill_level_mm = 3;

// how long hot water may run before shutting off, in seconds
const uint8_t de1_hot_water_length_s = 60;

class DE1 : public Device, public Machine {
 public:
  DE1(QueueHandle_t updateQ, QueueHandle_t cmdQ)
      : Device(DeviceType::machine, updateQ, cmdQ) {}

  bool stop() {
    if (!m_cmdChar) {
      return false;
    }
    return m_cmdChar->writeValue(de1_stop_cmd);
  }

  bool sleep() {
    if (!m_stateChar || !m_cmdChar) {
      return false;
    }

    // get our current state
    auto value = m_stateChar->readValue();

    if (value.length() != 2) {
      return false;
    }

    // if we are already going to sleep, skip this
    auto state = MachineState(value.data()[0]);
    if (state == MachineState::going_to_sleep || state == MachineState::sleep) {
      return true;
    }

    return m_cmdChar->writeValue(de1_sleep_cmd);
  }

  bool wake() {
    if (!m_cmdChar) {
      return false;
    }
    // TODO: turn off fan
    return true;
  }

  bool setRefillLevel(int mm) {
    if (mm < 1 || mm > 255) {
      return false;
    }
    m_refillLevelMm = mm;

    // if we are connected, update the machine immediately
    if (m_waterChar) {
      return writeRefillLevel();
    }
    return true;
  }

  bool setProfile(int idx) {
    if (idx < 0 || idx >= profile_count) {
      return false;
    }
    m_profileIdx = idx;

    // if we are connected, upload the profile immediately
    if (m_headerChar && m_frameChar) {
      return uploadProfile();
    }
    return true;
  }

  bool setFlushSeconds(int seconds) {
    if (seconds < 1 || seconds > 60) {
      return false;
    }
    m_flushSeconds = seconds;

    // if we are connected, update the machine immediately
    if (m_mmrChar) {
      return writeFlushTimeout();
    }
    return true;
  }

  bool setShotSettings(int steamTemp, int steamSeconds, int waterTemp, int waterVol) {
    if (steamTemp < 0 || steamTemp > 170 || steamSeconds < 0 || steamSeconds > 255 || waterTemp < 0 ||
        waterTemp > 100 || waterVol < 0 || waterVol > 500) {
      return false;
    }
    m_steamTemp = steamTemp;
    m_steamSeconds = steamSeconds;
    m_waterTemp = waterTemp;
    m_waterVol = waterVol;

    // if we are connected, update the machine immediately
    if (m_settingsChar) {
      return writeShotSettings();
    }
    return true;
  }

  // uploads a temporary water profile: one flow controlled frame at the
  // given mix temp, no pressure limit, long enough to cover the volume with
  // margin so the frame itself backstops a missing scale. the regular
  // profile should be re-uploaded via setProfile once the pour is done
  bool pourWater(int temp, int vol) {
    if (!m_headerChar || !m_frameChar || temp < 1 || temp > 100 || vol < 1 || vol > 500) {
      return false;
    }

    // one frame, no preinfuse, no minimum pressure, max flow 10 ml/s
    uint8_t header[5] = {1, 1, 0, 0, 0xA0};
    if (!m_headerChar->writeValue(header, sizeof(header), true)) {
      return false;
    }

    // flow control (CtrlF) at the mix temp (TMixTemp) ignoring the pressure
    // limit (IgnoreLimit), 6 ml/s, frame length covers the volume plus slack
    uint8_t secs = min(120, vol / 6 + 15);
    uint8_t frame[8] = {0, 0x51, 6 * 16, uint8_t(temp * 2), uint8_t(0x80 | secs), 0, 0, 0};
    return m_frameChar->writeValue(frame, sizeof(frame), true);
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
    if (length < 4) {
      return;
    }
    // two big-endian U16P8 fixed point values in mm: current level, then refill threshold
    int level = ((d[0] << 8) | d[1]) / 256;
    int threshold = ((d[2] << 8) | d[3]) / 256;
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
          m_cmdChar = ch;
        }

        if (ch->getUUID().toString() == de1_header_uuid) {
          m_headerChar = ch;
        }

        if (ch->getUUID().toString() == de1_frame_uuid) {
          m_frameChar = ch;
        }

        if (ch->getUUID().toString() == de1_mmr_write_uuid) {
          m_mmrChar = ch;
        }

        if (ch->getUUID().toString() == de1_shot_settings_uuid) {
          m_settingsChar = ch;
        }

        if (ch->canNotify()) {
          Serial.print("  CAN NOTIFY");
          // state update
          if (ch->getUUID().toString() == de1_state_uuid) {
            ch->subscribe(true, std::bind(&DE1::stateUpdate, this, std::placeholders::_1, std::placeholders::_2,
                                          std::placeholders::_3, std::placeholders::_4));
            Serial.print(" STATE");

            auto value = ch->readValue();

            if (value.length() != 2) {
              continue;
            }

            int state = value.data()[0];
            int subState = value.data()[1];
            queueUpdate(data::DataUpdate::newMachineStateUpdate((MachineState)state, (MachineSubstate)subState));

            m_stateChar = ch;
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

            // write our refill threshold
            m_waterChar = ch;
            if (writeRefillLevel()) {
              Serial.print(" REFILL SET");
            }

            // read the current level so we don't have to wait for the first notification
            auto value = ch->readValue();
            if (value.length() >= 4) {
              waterUpdate(ch, (uint8_t*)value.data(), value.length(), false);
            }
          }
        }
        Serial.println("");
      }
    }

    // set how long flushes should run
    if (m_mmrChar) {
      if (writeFlushTimeout()) {
        Serial.printf("[%s] set flush timeout: %ds\n", getName().c_str(), m_flushSeconds);
      }
    }

    // push our steam and hot water settings
    if (m_settingsChar) {
      if (writeShotSettings()) {
        Serial.printf("[%s] set shot settings: steam %dC %ds, water %dC %dml\n", getName().c_str(), m_steamTemp,
                      m_steamSeconds, m_waterTemp, m_waterVol);
      }
    }

    // load our selected profile onto the machine
    if (m_headerChar && m_frameChar) {
      if (uploadProfile()) {
        Serial.printf("[%s] uploaded profile: %s\n", getName().c_str(), profiles[m_profileIdx].name);
      } else {
        Serial.printf("[%s] failed uploading profile\n", getName().c_str());
      }
    }

    return true;
  }

  void teardownConnection(NimBLEClient* c) {
    m_waterChar = nullptr;
    m_headerChar = nullptr;
    m_frameChar = nullptr;
    m_mmrChar = nullptr;
    m_settingsChar = nullptr;
  }

  void selfRegister(Devices* devices) {
    devices->setMachine(this);
  }

  bool shouldConnect(NimBLEAdvertisedDevice* d) {
    // name returned by BLE is null terminated (in a std::string!) so fallback to strcmp
    return (strncmp(de1_name, d->getName().c_str(), strlen(de1_name))) == 0;
  }
  const std::string getName() {
    return de1_name;
  }

 private:
  bool writeRefillLevel() {
    // write is {level, threshold} as big-endian U16P8 values, level is ignored on write
    uint8_t levels[] = {0, 0, m_refillLevelMm, 0};
    return m_waterChar->writeValue(levels, sizeof(levels), true);
  }

  bool uploadProfile() {
    auto profile = profiles[m_profileIdx];

    if (!m_headerChar->writeValue(profile.header, 5, true)) {
      return false;
    }

    for (int i = 0; i < profile.frameCount; i++) {
      if (!m_frameChar->writeValue(profile.frames[i], 8, true)) {
        return false;
      }
    }

    return true;
  }

  // writes an MMR register, 4 byte little endian value
  bool writeMMR(uint32_t address, uint32_t value) {
    uint8_t packet[20] = {0};
    packet[0] = 4;  // length of the value in bytes
    packet[1] = (address >> 16) & 0xFF;
    packet[2] = (address >> 8) & 0xFF;
    packet[3] = address & 0xFF;
    packet[4] = value & 0xFF;
    packet[5] = (value >> 8) & 0xFF;
    packet[6] = (value >> 16) & 0xFF;
    packet[7] = (value >> 24) & 0xFF;
    return m_mmrChar->writeValue(packet, sizeof(packet), true);
  }

  bool writeFlushTimeout() {
    return writeMMR(de1_mmr_flush_timeout, m_flushSeconds * 10);
  }

  // the ShotSettings characteristic: a settings bitmask, steam temp and
  // length, hot water temp, volume and length, target espresso volume, then
  // a big endian U16P8 group temp (0, the profile owns group temp)
  bool writeShotSettings() {
    uint8_t packet[9] = {0};
    packet[1] = m_steamTemp;
    packet[2] = m_steamSeconds;
    packet[3] = m_waterTemp;
    // the volume field is a single byte, oversized settings split into even
    // rounds under the cap, one hot water press each
    auto vol = m_waterVol;
    if (vol > 250) {
      vol = m_waterVol / ((m_waterVol + 249) / 250);
    }
    packet[4] = vol;
    packet[5] = de1_hot_water_length_s;
    return m_settingsChar->writeValue(packet, sizeof(packet), true);
  }

  NimBLERemoteCharacteristic* m_settingsChar = nullptr;
  NimBLERemoteCharacteristic* m_cmdChar = nullptr;
  NimBLERemoteCharacteristic* m_stateChar = nullptr;
  NimBLERemoteCharacteristic* m_waterChar = nullptr;
  NimBLERemoteCharacteristic* m_headerChar = nullptr;
  NimBLERemoteCharacteristic* m_frameChar = nullptr;
  NimBLERemoteCharacteristic* m_mmrChar = nullptr;

  uint8_t m_refillLevelMm = de1_default_refill_level_mm;
  int m_profileIdx = 0;
  int m_flushSeconds = 3;
  int m_steamTemp = 150;
  int m_steamSeconds = 120;
  int m_waterTemp = 85;
  int m_waterVol = 120;
};

}  // namespace ble