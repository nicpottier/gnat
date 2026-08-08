#pragma once
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <profiles.h>

#define EEPROM_SIZE 1024

static const char* stop_weight_key = "stop_weight";
static const char* sleep_time_key = "sleep_time";
static const char* refill_level_key = "refill_level";
static const char* warn_level_key = "warn_level";
static const char* finer_direction_key = "finer_direction";
static const char* shot_margin_key = "shot_margin";
static const char* shot_target_key = "shot_target";
static const char* orientation_key = "orientation";
static const char* flush_seconds_key = "flush_seconds";
static const char* require_scale_key = "require_scale";
static const char* stop_lag_key = "lag";
static const char* lag_samples_key = "lagn";
static const char* profile_key = "profile";
static const char* enabled_key = "enabled";
static const char* error_key = "error";

static const char* invalid_sleep_time_error = "Invalid+sleep+time,+must+be+less+than+360";
static const char* invalid_stop_weight_error = "Invalid+stop+weight,+must+be+less+than+100";
static const char* invalid_refill_level_error = "Invalid+refill+level,+must+be+between+1+and+20";
static const char* invalid_warn_level_error = "Invalid+warning+level,+must+be+between+1+and+40";
static const char* invalid_shot_margin_error = "Invalid+shot+margin,+must+be+between+1+and+15";
static const char* invalid_flush_seconds_error = "Invalid+flush+time,+must+be+between+1+and+30";
static const char* invalid_shot_target_error = "Invalid+target+shot+time,+must+be+between+15+and+60";

static const int default_stop_weight = 36;
static const int max_stop_weight = 100;
static const int min_stop_weight = 0;

static const int default_sleep_time = 15;
static const int max_sleep_time = 360;
static const int min_sleep_time = 0;

static const int default_refill_level = 3;
static const int max_refill_level = 20;
static const int min_refill_level = 1;

static const int default_warn_level = 10;
static const int max_warn_level = 40;
static const int min_warn_level = 1;

static const int default_shot_margin = 3;
static const int max_shot_margin = 15;
static const int min_shot_margin = 1;

static const int default_shot_target = 30;
static const int max_shot_target = 60;
static const int min_shot_target = 15;

static const int default_flush_seconds = 3;
static const int max_flush_seconds = 30;
static const int min_flush_seconds = 1;

// how the device sits, flipped means rotated 180 degrees
static const int orientation_normal = 1;
static const int orientation_flipped = 2;
static const int default_orientation = orientation_normal;

// which way the grinder rotates for finer, 1 = left, 2 = right
static const int finer_direction_left = 1;
static const int finer_direction_right = 2;
static const int default_finer_direction = finer_direction_right;

// whether shots are stopped when no scale is connected
static const int require_scale_off = 1;
static const int require_scale_on = 2;
static const int default_require_scale = require_scale_on;

// how far ahead the predictive stop leads the scale, in tenths of a second,
// tuned automatically from settled shot weights
static const int default_stop_lag = 10;
static const int max_stop_lag = 30;
static const int min_stop_lag = 2;

// how many tuning samples have been folded into the stop lag, drives the
// adaptation gain down as experience accumulates
static const int max_lag_samples = 999;

// room for up to this many bytes of enabled profile mask (128 profiles)
static const int max_profile_mask_bytes = 16;

enum class ConfigError {
  none = 0,
  invalid_sleep_time,
  invalid_stop_weight,
  invalid_refill_level,
  invalid_warn_level,
  invalid_shot_margin,
  invalid_flush_seconds,
  invalid_shot_target,
};

class Config {
 public:
  Config()
      : m_sleepTime{default_sleep_time},
        m_stopWeight{default_stop_weight},
        m_refillLevel{default_refill_level},
        m_warnLevel{default_warn_level},
        m_finerDirection{default_finer_direction},
        m_shotMargin{default_shot_margin},
        m_shotTarget{default_shot_target},
        m_orientation{default_orientation},
        m_flushSeconds{default_flush_seconds},
        m_requireScale{default_require_scale},
        m_stopLag{default_stop_lag},
        m_lagSamples{0},
        m_profile{profile_default},
        m_error(ConfigError::none) {
    resetEnabled();
    m_version = millis();
  }

  static Config fromQueryString(char* query) {
    auto stopWeight = getUnsignedInt(query, stop_weight_key);
    auto sleepTime = getUnsignedInt(query, sleep_time_key);
    auto refillLevel = getUnsignedInt(query, refill_level_key);
    auto warnLevel = getUnsignedInt(query, warn_level_key);
    auto finerDirection = getUnsignedInt(query, finer_direction_key);
    auto shotMargin = getUnsignedInt(query, shot_margin_key);
    auto shotTarget = getUnsignedInt(query, shot_target_key);
    auto orientation = getUnsignedInt(query, orientation_key);
    auto flushSeconds = getUnsignedInt(query, flush_seconds_key);
    auto requireScale = getUnsignedInt(query, require_scale_key);
    auto stopLag = getUnsignedInt(query, stop_lag_key);
    auto lagSamples = getUnsignedInt(query, lag_samples_key);
    auto profile = getUnsignedInt(query, profile_key);

    char enabled[max_profile_mask_bytes * 2 + 1] = "";
    getStringValue(query, enabled_key, enabled, sizeof(enabled));

    return Config(sleepTime, stopWeight, refillLevel, warnLevel, finerDirection, shotMargin, shotTarget, orientation,
                  flushSeconds, requireScale, stopLag, lagSamples, profile, enabled);
  }

  static Config fromRequest(AsyncWebServerRequest* request) {
    int sleepTime = 0;
    int stopWeight = 0;
    int refillLevel = 0;

    auto param = request->getParam(sleep_time_key, true, false);
    if (param) {
      sleepTime = parseUnsignedInt(param->value().c_str());
    }

    param = request->getParam(stop_weight_key, true, false);
    if (param) {
      stopWeight = parseUnsignedInt(param->value().c_str());
    }

    param = request->getParam(refill_level_key, true, false);
    if (param) {
      refillLevel = parseUnsignedInt(param->value().c_str());
    }

    int warnLevel = 0;
    param = request->getParam(warn_level_key, true, false);
    if (param) {
      warnLevel = parseUnsignedInt(param->value().c_str());
    }

    int finerDirection = 0;
    param = request->getParam(finer_direction_key, true, false);
    if (param) {
      finerDirection = parseUnsignedInt(param->value().c_str());
    }

    int shotMargin = 0;
    param = request->getParam(shot_margin_key, true, false);
    if (param) {
      shotMargin = parseUnsignedInt(param->value().c_str());
    }

    int shotTarget = 0;
    param = request->getParam(shot_target_key, true, false);
    if (param) {
      shotTarget = parseUnsignedInt(param->value().c_str());
    }

    int orientation = 0;
    param = request->getParam(orientation_key, true, false);
    if (param) {
      orientation = parseUnsignedInt(param->value().c_str());
    }

    int flushSeconds = 0;
    param = request->getParam(flush_seconds_key, true, false);
    if (param) {
      flushSeconds = parseUnsignedInt(param->value().c_str());
    }

    int requireScale = 0;
    param = request->getParam(require_scale_key, true, false);
    if (param) {
      requireScale = parseUnsignedInt(param->value().c_str());
    }

    const char* enabled = "";
    param = request->getParam(enabled_key, true, false);
    if (param) {
      enabled = param->value().c_str();
    }

    // the web form doesn't carry the learned stop lag or its sample count,
    // callers copy those over
    return Config(sleepTime, stopWeight, refillLevel, warnLevel, finerDirection, shotMargin, shotTarget, orientation,
                  flushSeconds, requireScale, 0, 0, 0, enabled);
  }

  // returns a url encoded version of the config, suitable for writing to EEProm
  const char* toURLQuery(char* buffer, int size) {
    char* field = buffer;

    if (m_stopWeight != 0) {
      field += snprintf(field, size, "%s=%d&", stop_weight_key, m_stopWeight);
    }
    if (m_sleepTime != 0) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%d&", sleep_time_key, m_sleepTime);
    }
    if (m_refillLevel != 0) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%d&", refill_level_key, m_refillLevel);
    }
    if (m_warnLevel != 0) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%d&", warn_level_key, m_warnLevel);
    }
    if (m_finerDirection != 0) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%d&", finer_direction_key, m_finerDirection);
    }
    if (m_shotMargin != 0) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%d&", shot_margin_key, m_shotMargin);
    }
    if (m_shotTarget != 0) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%d&", shot_target_key, m_shotTarget);
    }
    if (m_orientation != 0) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%d&", orientation_key, m_orientation);
    }
    if (m_flushSeconds != 0) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%d&", flush_seconds_key, m_flushSeconds);
    }
    if (m_requireScale != 0) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%d&", require_scale_key, m_requireScale);
    }
    if (m_stopLag != 0) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%d&", stop_lag_key, m_stopLag);
    }
    if (m_lagSamples != 0) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%d&", lag_samples_key, m_lagSamples);
    }
    if (m_profile != 0) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%d&", profile_key, m_profile);
    }
    {
      char enabled[max_profile_mask_bytes * 2 + 1];
      size -= strlen(field);
      field += snprintf(field, size, "%s=%s&", enabled_key, enabledHex(enabled));
    }
    if (m_error == ConfigError::invalid_sleep_time) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%s", error_key, invalid_sleep_time_error);
    } else if (m_error == ConfigError::invalid_stop_weight) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%s", error_key, invalid_stop_weight_error);
    } else if (m_error == ConfigError::invalid_refill_level) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%s", error_key, invalid_refill_level_error);
    } else if (m_error == ConfigError::invalid_warn_level) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%s", error_key, invalid_warn_level_error);
    } else if (m_error == ConfigError::invalid_shot_margin) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%s", error_key, invalid_shot_margin_error);
    } else if (m_error == ConfigError::invalid_flush_seconds) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%s", error_key, invalid_flush_seconds_error);
    } else if (m_error == ConfigError::invalid_shot_target) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%s", error_key, invalid_shot_target_error);
    }
    return buffer;
  }

  int getSleepTime() {
    return m_sleepTime;
  }

  void setSleepTime(int minutes) {
    if (minutes < 1 || minutes > max_sleep_time) {
      return;
    }
    m_sleepTime = minutes;
    m_version = millis();
  }

  int getStopWeight() {
    return m_stopWeight;
  }

  void setStopWeight(int weight) {
    if (weight < 1 || weight > max_stop_weight) {
      return;
    }
    m_stopWeight = weight;
    m_version = millis();
  }

  int getRefillLevel() {
    return m_refillLevel;
  }

  // the water level (in mm) at which we suggest adding water
  int getWarnLevel() {
    return m_warnLevel;
  }

  void setWarnLevel(int level) {
    if (level < min_warn_level || level > max_warn_level) {
      return;
    }
    m_warnLevel = level;
    m_version = millis();
  }

  void setRefillLevel(int level) {
    if (level < min_refill_level || level > max_refill_level) {
      return;
    }
    m_refillLevel = level;
    m_version = millis();
  }

  // whether the grinder goes finer rotating left
  bool finerIsLeft() {
    return m_finerDirection != finer_direction_right;
  }

  int getFinerDirection() {
    return m_finerDirection;
  }

  void setFinerDirection(int direction) {
    if (direction < finer_direction_left || direction > finer_direction_right) {
      return;
    }
    m_finerDirection = direction;
    m_version = millis();
  }

  // how many seconds from the target shot time before we suggest a grind change
  int getShotMargin() {
    return m_shotMargin;
  }

  void setShotMargin(int margin) {
    if (margin < min_shot_margin || margin > max_shot_margin) {
      return;
    }
    m_shotMargin = margin;
    m_version = millis();
  }

  // the shot time we are aiming for
  int getShotTarget() {
    return m_shotTarget;
  }

  void setShotTarget(int target) {
    if (target < min_shot_target || target > max_shot_target) {
      return;
    }
    m_shotTarget = target;
    m_version = millis();
  }

  // whether the device sits rotated 180 degrees
  bool isFlipped() {
    return m_orientation == orientation_flipped;
  }

  int getOrientation() {
    return m_orientation;
  }

  void setOrientation(int orientation) {
    if (orientation < orientation_normal || orientation > orientation_flipped) {
      return;
    }
    m_orientation = orientation;
    m_version = millis();
  }

  // whether shots should be stopped when no scale is connected
  bool requireScale() {
    return m_requireScale != require_scale_off;
  }

  int getRequireScale() {
    return m_requireScale;
  }

  void setRequireScale(int requireScale) {
    if (requireScale < require_scale_off || requireScale > require_scale_on) {
      return;
    }
    m_requireScale = requireScale;
    m_version = millis();
  }

  // the predictive stop lead, in tenths of a second
  int getStopLag() {
    return m_stopLag;
  }

  double stopLagSeconds() {
    return m_stopLag / 10.0;
  }

  void setStopLag(int tenths) {
    if (tenths < min_stop_lag || tenths > max_stop_lag) {
      return;
    }
    m_stopLag = tenths;
    m_version = millis();
  }

  // how many tuning samples the stop lag has absorbed
  int getLagSamples() {
    return m_lagSamples;
  }

  void setLagSamples(int samples) {
    if (samples < 0 || samples > max_lag_samples) {
      return;
    }
    m_lagSamples = samples;
    m_version = millis();
  }

  // how long the machine should run a flush for
  int getFlushSeconds() {
    return m_flushSeconds;
  }

  void setFlushSeconds(int seconds) {
    if (seconds < min_flush_seconds || seconds > max_flush_seconds) {
      return;
    }
    m_flushSeconds = seconds;
    m_version = millis();
  }

  // the selected profile, 1-based
  int getProfile() {
    return m_profile;
  }

  void setProfile(int profile) {
    if (profile < 1 || profile > profile_count || !isProfileEnabled(profile - 1)) {
      profile = fallbackProfile();
    }
    m_profile = profile;
    m_version = millis();
  }

  bool isProfileEnabled(int idx) {
    if (idx < 0 || idx >= profile_count) {
      return false;
    }
    return (m_enabled[idx / 8] >> (idx % 8)) & 1;
  }

  int enabledProfileCount() {
    int count = 0;
    for (int idx = 0; idx < profile_count; idx++) {
      count += isProfileEnabled(idx);
    }
    return count;
  }

  // the next enabled profile after the current one, 1-based, wrapping around
  int nextEnabledProfile() {
    for (int i = 1; i <= profile_count; i++) {
      int idx = (m_profile - 1 + i) % profile_count;
      if (isProfileEnabled(idx)) {
        return idx + 1;
      }
    }
    return m_profile;
  }

  // the previous enabled profile before the current one, 1-based, wrapping around
  int prevEnabledProfile() {
    for (int i = 1; i <= profile_count; i++) {
      int idx = ((m_profile - 1 - i) % profile_count + profile_count) % profile_count;
      if (isProfileEnabled(idx)) {
        return idx + 1;
      }
    }
    return m_profile;
  }

  // the enabled mask as a hex string, buffer needs max_profile_mask_bytes * 2 + 1
  const char* enabledHex(char* buffer) {
    for (int i = 0; i < profile_mask_bytes; i++) {
      snprintf(buffer + i * 2, 3, "%02x", m_enabled[i]);
    }
    buffer[profile_mask_bytes * 2] = 0;
    return buffer;
  }

  ConfigError getError() {
    return m_error;
  }

  unsigned long getVersion() {
    return m_version;
  }

 private:
  Config(int sleepTime, int stopAtWeight, int refillLevel, int warnLevel, int finerDirection, int shotMargin,
         int shotTarget, int orientation, int flushSeconds, int requireScale, int stopLag, int lagSamples, int profile,
         const char* enabled)
      : m_sleepTime{sleepTime},
        m_stopWeight{stopAtWeight},
        m_refillLevel{refillLevel},
        m_warnLevel{warnLevel},
        m_finerDirection{finerDirection},
        m_shotMargin{shotMargin},
        m_shotTarget{shotTarget},
        m_orientation{orientation},
        m_flushSeconds{flushSeconds},
        m_requireScale{requireScale},
        m_stopLag{stopLag},
        m_lagSamples{lagSamples},
        m_profile{profile},
        m_error{ConfigError::none} {
    setEnabledFromHex(enabled);
    if (m_sleepTime == 0) {
      m_sleepTime = default_sleep_time;
    }

    else if (m_sleepTime < min_sleep_time || m_sleepTime > max_sleep_time) {
      m_error = ConfigError::invalid_sleep_time;
    }

    if (m_stopWeight == 0) {
      m_stopWeight = default_stop_weight;
    } else if (m_stopWeight < min_stop_weight || m_stopWeight > max_stop_weight) {
      m_error = ConfigError::invalid_stop_weight;
    }

    if (m_refillLevel == 0) {
      m_refillLevel = default_refill_level;
    } else if (m_refillLevel < min_refill_level || m_refillLevel > max_refill_level) {
      m_error = ConfigError::invalid_refill_level;
    }

    if (m_warnLevel == 0) {
      m_warnLevel = default_warn_level;
    } else if (m_warnLevel < min_warn_level || m_warnLevel > max_warn_level) {
      m_error = ConfigError::invalid_warn_level;
    }

    if (m_finerDirection < finer_direction_left || m_finerDirection > finer_direction_right) {
      m_finerDirection = default_finer_direction;
    }

    if (m_shotMargin == 0) {
      m_shotMargin = default_shot_margin;
    } else if (m_shotMargin < min_shot_margin || m_shotMargin > max_shot_margin) {
      m_error = ConfigError::invalid_shot_margin;
    }

    if (m_shotTarget == 0) {
      m_shotTarget = default_shot_target;
    } else if (m_shotTarget < min_shot_target || m_shotTarget > max_shot_target) {
      m_error = ConfigError::invalid_shot_target;
    }

    if (m_orientation < orientation_normal || m_orientation > orientation_flipped) {
      m_orientation = default_orientation;
    }

    if (m_requireScale < require_scale_off || m_requireScale > require_scale_on) {
      m_requireScale = default_require_scale;
    }

    if (m_stopLag < min_stop_lag || m_stopLag > max_stop_lag) {
      m_stopLag = default_stop_lag;
    }

    if (m_lagSamples < 0 || m_lagSamples > max_lag_samples) {
      m_lagSamples = 0;
    }

    if (m_flushSeconds == 0) {
      m_flushSeconds = default_flush_seconds;
    } else if (m_flushSeconds < min_flush_seconds || m_flushSeconds > max_flush_seconds) {
      m_error = ConfigError::invalid_flush_seconds;
    }

    if (m_profile == 0 || m_profile > profile_count || !isProfileEnabled(m_profile - 1)) {
      // unset, beyond our count (compiled in list changed) or no longer enabled
      m_profile = fallbackProfile();
    }

    m_version = millis();
  }

  // sets our enabled mask from a hex string, falling back to the default set
  void setEnabledFromHex(const char* hex) {
    if (strlen(hex) != size_t(profile_mask_bytes * 2)) {
      resetEnabled();
      return;
    }

    memset(m_enabled, 0, sizeof(m_enabled));
    bool any = false;
    for (int i = 0; i < profile_mask_bytes * 2; i++) {
      char c = hex[i];
      int nibble;
      if (c >= '0' && c <= '9') {
        nibble = c - '0';
      } else if (c >= 'a' && c <= 'f') {
        nibble = c - 'a' + 10;
      } else if (c >= 'A' && c <= 'F') {
        nibble = c - 'A' + 10;
      } else {
        resetEnabled();
        return;
      }
      m_enabled[i / 2] |= (i % 2) ? nibble : nibble << 4;
      any |= nibble != 0;
    }

    // an empty set would leave the button with nothing to cycle
    if (!any) {
      resetEnabled();
    }
  }

  void resetEnabled() {
    memset(m_enabled, 0, sizeof(m_enabled));
    memcpy(m_enabled, profile_default_mask, profile_mask_bytes);
  }

  // the profile to fall back to: the compiled in default if it's enabled,
  // otherwise the first enabled profile
  int fallbackProfile() {
    if (isProfileEnabled(profile_default - 1)) {
      return profile_default;
    }
    for (int idx = 0; idx < profile_count; idx++) {
      if (isProfileEnabled(idx)) {
        return idx + 1;
      }
    }
    return profile_default;
  }

  // parses the passed in string value into an unsigned integer,
  //    returns -1 if not int or greater than a 10000000
  static int parseUnsignedInt(const char* str) {
    // parse our buffer to an int
    char* end;
    auto value = strtol(str, &end, 10);

    // nothing was read, not a number
    if (end == str) {
      return -1;
    }

    // too big to be unsigned, error
    if (value > 1000000000) {
      return -1;
    }

    return int(value);
  }

  // finds the value of the key in the passed in query string and copies it
  // into the passed in buffer, returns the copied length, 0 if not found or
  // too big for the buffer
  static int getStringValue(const char* query, const char* key, char* buffer, int size) {
    buffer[0] = 0;
    auto start = strstr(query, key);
    if (start == nullptr) {
      return 0;
    }
    if (start != query && start[-1] != '&') {
      return 0;
    }
    if (start[strlen(key)] != '=') {
      return 0;
    }
    start += strlen(key) + 1;

    auto end = strstr(start, "&");
    if (end == nullptr) {
      end = start + strlen(start);
    }
    if (end - start >= size) {
      return 0;
    }

    strncpy(buffer, start, end - start);
    buffer[end - start] = 0;
    return end - start;
  }

  // finds the value of the key in the passed in query string.
  //    returns 0 if not found
  //    returns -1 if not valid int
  static int getUnsignedInt(const char* query, const char* key) {
    // find the start of the name
    auto start = strstr(query, key);

    // not found
    if (start == nullptr) {
      return 0;
    }

    // start needs to be first thing in string or be preceded by &
    if (start != query && start[-1] != '&') {
      return 0;
    }

    // end of start needs to be =
    if (start[strlen(key)] != '=') {
      return 0;
    }

    // move start to the start of our value
    start += strlen(key) + 1;

    // if that's the end of the string, no value
    if (!start) {
      return 0;
    }

    // find the end of the value
    auto end = strstr(start, "&");
    if (end == nullptr) {
      end = start + strlen(start);
    }

    // too big
    if (end - start > 11) {
      return 0;
    }

    char buffer[12];
    strncpy(buffer, start, end - start);
    buffer[end - start] = 0;
    auto value = parseUnsignedInt(buffer);

    return value;
  }

  // how long before we put the machine to sleep
  int m_sleepTime;

  // the weight we will stop at
  int m_stopWeight;

  // the water level (in mm) at which the machine asks for a refill
  int m_refillLevel;

  // the water level (in mm) at which we suggest adding water
  int m_warnLevel;

  // which way the grinder rotates for finer
  int m_finerDirection;

  // seconds from the target shot time before we suggest a grind change
  int m_shotMargin;

  // the shot time we are aiming for, in seconds
  int m_shotTarget;

  // how the device sits
  int m_orientation;

  // how long the machine should run a flush for, in seconds
  int m_flushSeconds;

  // whether shots are stopped when no scale is connected
  int m_requireScale;

  // the predictive stop lead in tenths of a second, auto tuned
  int m_stopLag;

  // how many tuning samples the lag has absorbed
  int m_lagSamples;

  // the selected profile, 1-based index into the compiled in profiles
  int m_profile;

  // bitmask of which profiles are enabled for cycling
  uint8_t m_enabled[max_profile_mask_bytes];

  // the last error encountered while parsing
  ConfigError m_error;

  // the version for this config, used to detect changes by widgets
  unsigned long m_version;
};

bool writeConfig(Config config) {
  char query[EEPROM_SIZE - 3];
  config.toURLQuery(query, EEPROM_SIZE - 3);

  // two byte little endian length, then our query string (including null)
  uint16_t length = strlen(query);
  EEPROM.write(0, length & 0xFF);
  EEPROM.write(1, (length >> 8) & 0xFF);
  EEPROM.writeString(2, query);

  Serial.printf("wrote to EEPROM: %s\n", query);

  return EEPROM.commit();
}

Config readConfig() {
  auto config = Config();

  EEPROM.begin(EEPROM_SIZE);

  // two byte little endian length at 0, string from 2
  uint16_t length = EEPROM.read(0) | (EEPROM.read(1) << 8);
  int offset = 2;

  // configs written before the two byte header had a single length byte with
  // the string starting right behind it, the string's first character lands
  // in our high byte and makes the length impossibly large, read those the
  // old way and rewrite them in the new format below
  bool legacy = false;
  if (length < 1 || length > EEPROM_SIZE - 3) {
    uint8_t old = EEPROM.read(0);
    if (old < 1 || old >= 255) {
      return config;
    }
    length = old;
    offset = 1;
    legacy = true;
  }

  char buffer[EEPROM_SIZE + 1];
  auto read = EEPROM.readString(offset, buffer, length);
  buffer[read] = 0;

  // set our config to the parsed value
  config = Config::fromQueryString(buffer);
  char query[EEPROM_SIZE - 3];
  config.toURLQuery(query, EEPROM_SIZE - 3);
  Serial.printf("eeprom: %s config: %s\n", buffer, query);

  // if there was an error reading it, reset to default
  if (config.getError() != ConfigError::none) {
    config = Config();
  } else if (legacy) {
    Serial.println("migrating config to two byte length format");
    writeConfig(config);
  }

  return config;
}