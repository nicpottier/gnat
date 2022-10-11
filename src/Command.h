#pragma once

#include <ble/Machine.h>
#include <ble/Scale.h>

namespace cmd {

enum class CommandType {
  EMPTY = 0,
  SCALE_INIT,
  SCALE_TARE,
  SCALE_DISPLAY,
  SCALE_GRAMS,
  MACHINE_SLEEP,
  MACHINE_STOP,
};

class Devices {
 public:
  ble::Scale *getScale() { return m_scale; }
  ble::Machine *getMachine() { return m_machine; }

  void setScale(ble::Scale *s) { m_scale = s; }
  void setMachine(ble::Machine *m) { m_machine = m; }

 private:
  ble::Machine *m_machine = nullptr;
  ble::Scale *m_scale = nullptr;
};

class TareScaleCommand {
 public:
  TareScaleCommand(){};

  bool execute(Devices *devices) {
    auto s = devices->getScale();
    if (!s) return false;
    Serial.println("SENDING SCALE TARE");
    return s->tare();
  }
};

class InitScaleCommand {
 public:
  InitScaleCommand(){};

  bool execute(Devices *devices) {
    auto s = devices->getScale();
    if (!s) return false;
    Serial.println("SENDING SCALE INIT");
    return s->init();
  }
};

class StopMachineCommand {
 public:
  StopMachineCommand(){};

  bool execute(Devices *devices) {
    auto m = devices->getMachine();
    if (!m) return false;
    Serial.println("SENDING MACHINE STOP");
    return m->stop();
  }
};

class SleepMachineCommand {
 public:
  SleepMachineCommand(){};

  bool execute(Devices *devices) {
    auto m = devices->getMachine();
    if (!m) return false;
    Serial.println("SENDING MACHINE SLEEP");
    return m->sleep();
  }
};

class CommandRequest {
 public:
  CommandRequest(CommandType type) : m_type{type} {}

  bool execute(Devices *devices) {
    switch (m_type) {
      case CommandType::SCALE_INIT:
        return m_initScale.execute(devices);
      case CommandType::SCALE_TARE:
        return m_tareScale.execute(devices);
      case CommandType::MACHINE_STOP:
        return m_stopMachine.execute(devices);
      case CommandType::MACHINE_SLEEP:
        return m_sleepMachine.execute(devices);
      case CommandType::EMPTY:
        return true;
      default:
        return false;
    }
  }

  CommandType getType() { return m_type; }

  static CommandRequest newTareScaleCommand() {
    auto c = CommandRequest{CommandType::SCALE_TARE};
    c.m_tareScale = TareScaleCommand{};
    return c;
  }

  static CommandRequest newInitScaleCommand() {
    auto c = CommandRequest{CommandType::SCALE_INIT};
    c.m_initScale = InitScaleCommand{};
    return c;
  }

  static CommandRequest newStopMachineCommand() {
    auto c = CommandRequest{CommandType::MACHINE_STOP};
    c.m_stopMachine = StopMachineCommand{};
    return c;
  }

  static CommandRequest newSleepMachineCommand() {
    auto c = CommandRequest{CommandType::MACHINE_SLEEP};
    c.m_sleepMachine = SleepMachineCommand{};
    return c;
  }

 private:
  CommandType m_type;
  union {
    TareScaleCommand m_tareScale;
    InitScaleCommand m_initScale;
    StopMachineCommand m_stopMachine;
    SleepMachineCommand m_sleepMachine;
  };
};

}  // namespace cmd