#pragma once

const long restart_tick_delay = 100;

enum class UpdateType : uint8_t {
  null_update,
  ble_state_update,
  weight_update,
  sample_update,
  machine_state_update,
  restart_update,
  water_level_update,
  screen_update,
};

enum class ScreenID : uint8_t { unknown, splash, brew, config };

enum class DeviceType : uint8_t { unknown, machine, scale, last };

enum class BLEState : uint8_t {
  unknown,
  disconnected,
  connecting,
  connected,
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
  stabilizing = 3,
  preinfusing = 4,
  pouring = 5,
  ending = 6,
  steaming = 7,
  descale_init = 8,
  descale_fill_group = 9,
  descale_return = 10,
  descale_group = 11,
  descale_steam = 12,
  clean_init = 13,
  clean_fill_group = 14,
  clean_soak = 15,
  clean_group = 16,
  refill = 17,
  paused_steam = 18,
  user_not_present = 19,
  puffing = 20,
  error_nan = 200,
  error_inf = 201,
  error_generic = 202,
  error_aCC = 203,
  error_tsensor = 204,
  error_psensor = 205,
  error_wlevel = 206,
  error_dip = 207,
  error_assertion = 208,
  error_unsafe = 209,
  error_invalid_parm = 210,
  error_flash = 211,
  error_oom = 212,
  error_deadline = 213,
  error_hi_current = 214,
  error_lo_current = 215,
  error_boot_fill = 216,
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
  BLEState getMachineBLEState() {
    return bleStates[int(DeviceType::machine)];
  }
  BLEState getScaleBLEState() {
    return bleStates[int(DeviceType::scale)];
  }

  void init() {
    splashEnd = millis() + 2000;
  }

  BLEState bleStates[int(DeviceType::last)];
  Sample lastSample{};
  double currentWeight = 0;

  unsigned long tickID = 0;
  unsigned long restartTickID = 0;

  MachineState machineState = MachineState::unknown;
  MachineSubstate machineSubstate = MachineSubstate::unknown;

  int waterLevel = 0;
  int waterLevelThreshold = 0;

  ScreenID screen = ScreenID::splash;
  unsigned long splashEnd = 0;
};

class ScreenUpdate {
 public:
  ScreenUpdate(ScreenID screen)
      : m_screen{screen} {}

  bool apply(Context* ctx) {
    ctx->screen = m_screen;
    return true;
  }

 private:
  ScreenID m_screen;
};

class WaterLevelUpdate {
 public:
  WaterLevelUpdate(int level, int threshold)
      : m_waterLevel{level},
        m_waterLevelThreshold{threshold} {}

  bool apply(Context* ctx) {
    ctx->waterLevel = m_waterLevel;
    ctx->waterLevelThreshold = m_waterLevelThreshold;
    return true;
  }

 private:
  int m_waterLevel;
  int m_waterLevelThreshold;
};

class BLEStateUpdate {
 public:
  BLEStateUpdate(DeviceType device, BLEState state)
      : m_device{device},
        m_state{state} {}

  bool apply(Context* ctx) {
    if (m_device >= DeviceType::last) {
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
  WeightUpdate(double weight)
      : m_weight{weight} {}
  bool apply(Context* ctx) {
    ctx->currentWeight = m_weight;
    return true;
  }

 private:
  double m_weight;
};

class SampleUpdate {
 public:
  SampleUpdate(Sample s)
      : m_sample{s} {}
  bool apply(Context* ctx) {
    ctx->lastSample = m_sample;
    return true;
  }

 private:
  Sample m_sample;
};

class RestartUpdate {
 public:
  RestartUpdate() {}

  bool apply(Context* ctx) {
    ctx->restartTickID = ctx->tickID + restart_tick_delay;
    return true;
  }
};

class MachineStateUpdate {
 public:
  MachineStateUpdate()
      : m_state{MachineState::unknown},
        m_substate{MachineSubstate::unknown} {};
  MachineStateUpdate(MachineState state, MachineSubstate substate)
      : m_state{state},
        m_substate{substate} {};

  bool apply(Context* ctx) {
    ctx->machineState = m_state;
    ctx->machineSubstate = m_substate;
    Serial.printf("APPLIED STATE UPDATE: %d [%d]\n", int(m_state), int(m_substate));
    return true;
  }

 private:
  MachineState m_state;
  MachineSubstate m_substate;
};

class DataUpdate {
 public:
  DataUpdate(UpdateType type)
      : m_type{type} {}

  bool apply(Context* ctx) {
    switch (m_type) {
      case UpdateType::ble_state_update:
        return m_bleStateUpdate.apply(ctx);
      case UpdateType::weight_update:
        return m_weightUpdate.apply(ctx);
      case UpdateType::sample_update:
        return m_sampleUpdate.apply(ctx);
      case UpdateType::machine_state_update:
        return m_machineStateUpdate.apply(ctx);
      case UpdateType::restart_update:
        return m_restartUpdate.apply(ctx);
      case UpdateType::water_level_update:
        return m_waterLevelUpdate.apply(ctx);
      case UpdateType::screen_update:
        return m_screenUpdate.apply(ctx);
      default:
        return false;
    }
  }

  static DataUpdate newConnectionStatus(DeviceType d, BLEState s) {
    auto u = DataUpdate{UpdateType::ble_state_update};
    u.m_bleStateUpdate = BLEStateUpdate(d, s);
    return u;
  }

  static DataUpdate newWeightCommand(double weight) {
    auto u = DataUpdate{UpdateType::weight_update};
    u.m_weightUpdate = WeightUpdate{weight};
    return u;
  }

  static DataUpdate newSampleUpdate(Sample sample) {
    auto u = DataUpdate{UpdateType::sample_update};
    u.m_sampleUpdate = SampleUpdate{sample};
    return u;
  }

  static DataUpdate newMachineStateUpdate(MachineState state, MachineSubstate subState) {
    auto u = DataUpdate{UpdateType::machine_state_update};
    u.m_machineStateUpdate = MachineStateUpdate{state, subState};
    return u;
  }

  static DataUpdate newRestartUpdate() {
    auto u = DataUpdate{UpdateType::restart_update};
    u.m_restartUpdate = RestartUpdate{};
    return u;
  }

  static DataUpdate newWaterLevelUpdate(int level, int threshold) {
    auto u = DataUpdate{UpdateType::water_level_update};
    u.m_waterLevelUpdate = WaterLevelUpdate{level, threshold};
    return u;
  }

  static DataUpdate newScreenUpdate(ScreenID screen) {
    auto u = DataUpdate{UpdateType::screen_update};
    u.m_screenUpdate = ScreenUpdate{screen};
    return u;
  }

 private:
  UpdateType m_type;

  union {
    BLEStateUpdate m_bleStateUpdate;
    WeightUpdate m_weightUpdate;
    SampleUpdate m_sampleUpdate;
    MachineStateUpdate m_machineStateUpdate;
    RestartUpdate m_restartUpdate;
    WaterLevelUpdate m_waterLevelUpdate;
    ScreenUpdate m_screenUpdate;
  };
};

}  // namespace data