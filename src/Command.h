#pragma once

#include <ble/Devices.h>

namespace cmd {

enum class CommandType {
  EMPTY = 0,
  SCALE_INIT,
  SCALE_TARE,
  SCALE_DISPLAY,
  SCALE_GRAMS,
  SLEEP,
  WAKE,
  MACHINE_STOP,
  MACHINE_REFILL_LEVEL,
  MACHINE_PROFILE,
  MACHINE_FLUSH_SECONDS,
};

class TareScaleCommand {
 public:
  TareScaleCommand(){};

  bool execute(ble::Devices *devices) {
    auto s = devices->getScale();
    if (!s) return false;
    Serial.println("SENDING SCALE TARE");
    return s->tare();
  }
};

class InitScaleCommand {
 public:
  InitScaleCommand(){};

  bool execute(ble::Devices *devices) {
    auto s = devices->getScale();
    if (!s) return false;
    Serial.println("SENDING SCALE INIT");
    return s->init();
  }
};

class StopMachineCommand {
 public:
  StopMachineCommand(){};

  bool execute(ble::Devices *devices) {
    auto m = devices->getMachine();
    if (!m) return false;
    Serial.println("SENDING MACHINE STOP");
    return m->stop();
  }
};

class RefillLevelCommand {
 public:
  RefillLevelCommand()
      : m_level{0} {};
  RefillLevelCommand(int level)
      : m_level{level} {};

  bool execute(ble::Devices *devices) {
    auto m = devices->getMachine();
    if (!m) return false;
    Serial.printf("SENDING REFILL LEVEL %d\n", m_level);
    return m->setRefillLevel(m_level);
  }

 private:
  int m_level;
};

class ProfileCommand {
 public:
  ProfileCommand()
      : m_idx{0} {};
  ProfileCommand(int idx)
      : m_idx{idx} {};

  bool execute(ble::Devices *devices) {
    auto m = devices->getMachine();
    if (!m) return false;
    Serial.printf("SENDING PROFILE %d\n", m_idx);
    return m->setProfile(m_idx);
  }

 private:
  int m_idx;
};

class FlushSecondsCommand {
 public:
  FlushSecondsCommand()
      : m_seconds{0} {};
  FlushSecondsCommand(int seconds)
      : m_seconds{seconds} {};

  bool execute(ble::Devices *devices) {
    auto m = devices->getMachine();
    if (!m) return false;
    Serial.printf("SENDING FLUSH SECONDS %d\n", m_seconds);
    return m->setFlushSeconds(m_seconds);
  }

 private:
  int m_seconds;
};

class SleepCommand {
 public:
  SleepCommand(){};

  bool execute(ble::Devices *devices) {
    auto success = false;
    auto m = devices->getMachine();
    if (m) {
      success = m->sleep();
    }

    auto s = devices->getScale();
    if (s) {
      success &= s->sleep();
    }

    return success;
  }
};

class WakeCommand {
 public:
  WakeCommand(){};

  bool execute(ble::Devices *devices) {
    auto success = false;
    auto m = devices->getMachine();
    if (m) {
      success = m->wake();
    }

    auto s = devices->getScale();
    if (s) {
      success &= s->wake();
    }

    return success;
  }
};

class CommandRequest {
 public:
  CommandRequest(CommandType type)
      : m_type{type} {}

  bool execute(ble::Devices *devices) {
    switch (m_type) {
      case CommandType::SCALE_INIT:
        return m_initScale.execute(devices);
      case CommandType::SCALE_TARE:
        return m_tareScale.execute(devices);
      case CommandType::MACHINE_STOP:
        return m_stopMachine.execute(devices);
      case CommandType::MACHINE_REFILL_LEVEL:
        return m_refillLevel.execute(devices);
      case CommandType::MACHINE_PROFILE:
        return m_profile.execute(devices);
      case CommandType::MACHINE_FLUSH_SECONDS:
        return m_flushSeconds.execute(devices);
      case CommandType::SLEEP:
        return m_sleep.execute(devices);
      case CommandType::WAKE:
        return m_wake.execute(devices);
      case CommandType::EMPTY:
        return true;
      default:
        return false;
    }
  }

  CommandType getType() {
    return m_type;
  }

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

  static CommandRequest newRefillLevelCommand(int level) {
    auto c = CommandRequest{CommandType::MACHINE_REFILL_LEVEL};
    c.m_refillLevel = RefillLevelCommand{level};
    return c;
  }

  static CommandRequest newProfileCommand(int idx) {
    auto c = CommandRequest{CommandType::MACHINE_PROFILE};
    c.m_profile = ProfileCommand{idx};
    return c;
  }

  static CommandRequest newFlushSecondsCommand(int seconds) {
    auto c = CommandRequest{CommandType::MACHINE_FLUSH_SECONDS};
    c.m_flushSeconds = FlushSecondsCommand{seconds};
    return c;
  }

  static CommandRequest newSleepCommand() {
    auto c = CommandRequest{CommandType::SLEEP};
    c.m_sleep = SleepCommand{};
    return c;
  }

  static CommandRequest newWakeCommand() {
    auto c = CommandRequest{CommandType::WAKE};
    c.m_wake = WakeCommand{};
    return c;
  }

 private:
  CommandType m_type;
  union {
    TareScaleCommand m_tareScale;
    InitScaleCommand m_initScale;
    StopMachineCommand m_stopMachine;
    RefillLevelCommand m_refillLevel;
    ProfileCommand m_profile;
    FlushSecondsCommand m_flushSeconds;
    SleepCommand m_sleep;
    WakeCommand m_wake;
  };
};

}  // namespace cmd