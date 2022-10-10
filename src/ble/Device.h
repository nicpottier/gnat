#pragma once

#include <Command.h>
#include <Data.h>
#include <NimBLEDevice.h>

namespace ble {

class Device : public NimBLEClientCallbacks {
 public:
  Device(DeviceType t, QueueHandle_t updateQ, QueueHandle_t cmdQ) : m_type{t}, m_updateQ{updateQ}, m_cmdQ{cmdQ} {}

  virtual bool setupConnection(NimBLEClient* c) = 0;
  virtual void teardownConnection(NimBLEClient* c) = 0;
  virtual bool shouldConnect(NimBLEAdvertisedDevice* d) = 0;
  virtual const std::string getName() = 0;

  void onConnect(NimBLEClient* c) {
    Serial.printf("[%d][%s] connected to %s\n", xPortGetCoreID(), getName().c_str(),
                  c->getPeerAddress().toString().c_str());
    if (!setupConnection(c)) {
      Serial.printf("[%d][%s] connection setup failed, disconnecting\n", xPortGetCoreID(), getName().c_str());
    }
  };

  void onDisconnect(NimBLEClient* c) {
    Serial.printf("[%d][%s] disconnected from %s\n", xPortGetCoreID(), getName().c_str(),
                  c->getPeerAddress().toString().c_str());
    m_client = nullptr;
    teardownConnection(c);
    setState(BLEState::DISCONNECTED);
    NimBLEDevice::deleteClient(c);
  };

  bool isDisconnected() { return (m_state == BLEState::DISCONNECTED || !m_client || !m_client->isConnected()); }

  bool connect(NimBLEClient* c) {
    c->setClientCallbacks(this, false);
    setState(BLEState::CONNECTING);
    if (c->connect() && c->isConnected()) {
      setState(BLEState::CONNECTED);
      m_client = c;
      return true;
    } else {
      m_client = nullptr;
      setState(BLEState::DISCONNECTED);
      return false;
    }
  }

  DeviceType getType() { return m_type; }

  void queueCommand(cmd::CommandRequest c) {
    if (xQueueSend(m_cmdQ, &c, 10) != pdTRUE) {
      Serial.println(" !!! failed sending command");
    }
  }

  void queueUpdate(data::DataUpdate d) {
    if (xQueueSend(m_updateQ, &d, 10) != pdTRUE) {
      Serial.println(" !!! failed sending update");
    }
  }

 private:
  void setState(BLEState s) {
    m_state = s;
    queueUpdate(data::DataUpdate::newConnectionStatus(m_type, m_state));
  }

  NimBLEClient* m_client = nullptr;
  BLEState m_state;
  DeviceType m_type;
  QueueHandle_t m_updateQ;
  QueueHandle_t m_cmdQ;
};
};  // namespace ble
