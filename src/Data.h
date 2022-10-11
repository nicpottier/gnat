#pragma once

enum class UpdateType : uint8_t {
  EMPTY_UPDATE,
  BLE_STATE_UPDATE,
  WEIGHT_UPDATE,
  SAMPLE_UPDATE,
  MACHINE_STATE_UPDATE,
};

enum class DeviceType : uint8_t { UNKNOWN, MACHINE, SCALE, LAST };

enum class BLEState : uint8_t {
  UNKNOWN = 0,
  DISCONNECTED,
  CONNECTING,
  CONNECTED,
};

enum class MachineState {
  sleep = 0,           // 0 Everything is off
  going_to_sleep = 1,  // 1 Going to sleep
  idle = 2,            // 2 Heaters are controlled, tank water will be heated if required.
  busy = 3,            // 3 Firmware is doing something you can't interrupt (eg. cooling down water heater after a shot,
                       // calibrating sensors on startup).
  espresso = 4,        // 4 Making espresso
  steam = 5,           // 5 Making steam
  hot_water = 6,       // 6 Making hot water
  short_cal = 7,       // 7 Running a short calibration
  self_test = 8,       // 8 Checking as much as possible within the firmware. Probably only used during manufacture or
                       // repair.
  long_cal = 9,       // 9 Long and involved calibration, possibly involving user interaction. (See substates below, for
                      // cases like that).
  descale = 10,       // A Descale the whole bang-tooty
  fatal_error = 11,   // B Something has gone horribly wrong
  init = 12,          // C Machine has not been run yet
  no_request = 13,    // D State for T_RequestedState. Means nothing is specifically requested
  skip_to_next = 14,  // E In Espresso, skip to next frame. Others, go to Idle if possible
  hot_water_rinse = 15,  // F Produce hot water at whatever temperature is available
  steam_rinse = 16,      // 10 Produce a blast of steam
  refill = 17,           // 11 Attempting, or needs, a refill.
  clean = 18,            // 12 Clean group head
  in_boot_loader = 19,   // 13 The main firmware has not run for some reason. Bootloader is active.
  air_purge = 20,        // 14 Air purge.
  sched_idle = 21,       // 15 Scheduled wake up idle state
  unknown = 255,         // 255, unknown (gnat specific)
};

const char* STATES[] = {
    "sleep",           // 0 Everything is off
    "going_to_sleep",  // 1 Going to sleep
    "idle",            // 2 Heaters are controlled, tank water will be heated if required.
    "busy",            // 3 Firmware is doing something you can't interrupt (eg. cooling down water heater after a shot,
                       // calibrating sensors on startup).
    "espresso",        // 4 Making espresso
    "steam",           // 5 Making steam
    "hot_water",       // 6 Making hot water
    "short_cal",       // 7 Running a short calibration
    "self_test",       // 8 Checking as much as possible within the firmware. Probably only used during manufacture or
                       // repair.
    "long_cal",      // 9 Long and involved calibration, possibly involving user interaction. (See substates below, for
                     // cases like that).
    "descale",       // A Descale the whole bang-tooty
    "fatal_error",   // B Something has gone horribly wrong
    "init",          // C Machine has not been run yet
    "no_request",    // D State for T_RequestedState. Means nothing is specifically requested
    "skip_to_next",  // E In Espresso, skip to next frame. Others, go to Idle if possible
    "hot_water_rinse",  // F Produce hot water at whatever temperature is available
    "steam_rinse",      // 10 Produce a blast of steam
    "refill",           // 11 Attempting, or needs, a refill.
    "clean",            // 12 Clean group head
    "in_boot_loader",   // 13 The main firmware has not run for some reason. Bootloader is active.
    "air_purge",        // 14 Air purge.
    "sched_idle",       // 15 Scheduled wake up idle state
};

enum class MachineSubstate {
  ready = 0,
  heating = 1,
  final_heating = 2,
  stabilising = 3,
  preinfusion = 4,
  pouring = 5,
  ending = 6,
  Steaming = 7,
  DescaleInit = 8,
  DescaleFillGroup = 9,
  DescaleReturn = 10,
  DescaleGroup = 11,
  DescaleSteam = 12,
  CleanInit = 13,
  CleanFillGroup = 14,
  CleanSoak = 15,
  CleanGroup = 16,
  refill = 17,
  PausedSteam = 18,
  UserNotPresent = 19,
  puffing = 20,
  Error_NaN = 200,
  Error_Inf = 201,
  Error_Generic = 202,
  Error_ACC = 203,
  Error_TSensor = 204,
  Error_PSensor = 205,
  Error_WLevel = 206,
  Error_DIP = 207,
  Error_Assertion = 208,
  Error_Unsafe = 209,
  Error_InvalidParm = 210,
  Error_Flash = 211,
  Error_OOM = 212,
  Error_Deadline = 213,
  Error_HiCurrent = 214,
  Error_LoCurrent = 215,
  Error_BootFill = 216,
  unknown = 255,  // 255, unknown (gnat specific)
};

namespace data {
class Sample {
 public:
  Sample()
      : sampleTime{0},
        groupPressure{0},
        groupFlow{0},
        mixTemp{0},
        headTemp{0},
        targetMixTemp{0},
        targetHeadTemp{0},
        targetGroupPressure{0},
        targetGroupFlow{0},
        frameNumber{0},
        steamTemp{0} {}

  Sample(int sampleTime, double groupPressure, double groupFlow, double mixTemp, double headTemp, double targetMixTemp,
         double targetHeadTemp, double targetGroupPressure, double targetGroupFlow, int frameNumber, int steamTemp)
      : sampleTime{sampleTime},
        groupPressure{groupPressure},
        groupFlow{groupFlow},
        mixTemp{mixTemp},
        headTemp{headTemp},
        targetMixTemp{targetMixTemp},
        targetHeadTemp{targetHeadTemp},
        targetGroupPressure{targetGroupPressure},
        targetGroupFlow{targetGroupFlow},
        frameNumber{frameNumber},
        steamTemp{steamTemp} {}

  int sampleTime;
  double groupPressure;
  double groupFlow;
  double mixTemp;
  double headTemp;
  double targetMixTemp;
  double targetHeadTemp;
  double targetGroupPressure;
  double targetGroupFlow;
  int frameNumber;
  int steamTemp;
};

class Context {
 public:
  BLEState getMachineBLEState() { return bleStates[int(DeviceType::MACHINE)]; }
  BLEState getScaleBLEState() { return bleStates[int(DeviceType::SCALE)]; }

  BLEState bleStates[int(DeviceType::LAST)];
  Sample lastSample{};
  double currentWeight = 0;

  MachineState machineState = MachineState::unknown;
  MachineSubstate machineSubstate = MachineSubstate::unknown;
};

class BLEStateUpdate {
 public:
  BLEStateUpdate(DeviceType device, BLEState state) : m_device{device}, m_state{state} {}

  bool apply(Context* ctx) {
    if (m_device >= DeviceType::LAST) {
      return false;
    }

    ctx->bleStates[int(m_device)] = m_state;
    return true;
  }

 private:
  DeviceType m_device;
  BLEState m_state;
};

class WeightUpdate {
 public:
  WeightUpdate(double weight) : m_weight{weight} {}
  bool apply(Context* ctx) {
    ctx->currentWeight = m_weight;
    return true;
  }

 private:
  double m_weight;
};

class SampleUpdate {
 public:
  SampleUpdate(Sample s) : m_sample{s} {}
  bool apply(Context* ctx) {
    ctx->lastSample = m_sample;
    return true;
  }

 private:
  Sample m_sample;
};

class MachineStateUpdate {
 public:
  MachineStateUpdate() : m_state{MachineState::unknown}, m_substate{MachineSubstate::unknown} {};
  MachineStateUpdate(MachineState state, MachineSubstate substate) : m_state{state}, m_substate{substate} {};

  bool apply(Context* ctx) {
    ctx->machineState = m_state;
    ctx->machineSubstate = m_substate;
    Serial.printf("APPLIED STATE UPDATE: %d [%d]\n", m_state, m_substate);
    return true;
  }

 private:
  MachineState m_state;
  MachineSubstate m_substate;
};

class DataUpdate {
 public:
  DataUpdate(UpdateType type) : m_type{type} {}

  bool apply(Context* ctx) {
    switch (m_type) {
      case UpdateType::BLE_STATE_UPDATE:
        return m_bleStateUpdate.apply(ctx);
      case UpdateType::WEIGHT_UPDATE:
        return m_weightUpdate.apply(ctx);
      case UpdateType::SAMPLE_UPDATE:
        return m_sampleUpdate.apply(ctx);
      case UpdateType::MACHINE_STATE_UPDATE:
        return m_machineStateUpdate.apply(ctx);
      default:
        return false;
    }
  }

  static DataUpdate newConnectionStatus(DeviceType d, BLEState s) {
    auto u = DataUpdate{UpdateType::BLE_STATE_UPDATE};
    u.m_bleStateUpdate = BLEStateUpdate(d, s);
    return u;
  }

  static DataUpdate newWeightCommand(double weight) {
    auto u = DataUpdate{UpdateType::WEIGHT_UPDATE};
    u.m_weightUpdate = WeightUpdate{weight};
    return u;
  }

  static DataUpdate newSampleUpdate(Sample sample) {
    auto u = DataUpdate{UpdateType::SAMPLE_UPDATE};
    u.m_sampleUpdate = SampleUpdate{sample};
    return u;
  }

  static DataUpdate newMachineStateUpdate(MachineState state, MachineSubstate subState) {
    auto u = DataUpdate{UpdateType::MACHINE_STATE_UPDATE};
    u.m_machineStateUpdate = MachineStateUpdate{state, subState};
    return u;
  }

 private:
  UpdateType m_type;

  union {
    BLEStateUpdate m_bleStateUpdate;
    WeightUpdate m_weightUpdate;
    SampleUpdate m_sampleUpdate;
    MachineStateUpdate m_machineStateUpdate;
  };
};

}  // namespace data