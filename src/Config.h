#pragma once
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <profiles.h>

#define EEPROM_SIZE 256

static const char* stop_weight_key = "stop_weight";
static const char* sleep_time_key = "sleep_time";
static const char* refill_level_key = "refill_level";
static const char* profile_key = "profile";
static const char* enabled_key = "enabled";
static const char* error_key = "error";

static const char* invalid_sleep_time_error = "Invalid+sleep+time,+must+be+less+than+360";
static const char* invalid_stop_weight_error = "Invalid+stop+weight,+must+be+less+than+100";
static const char* invalid_refill_level_error = "Invalid+refill+level,+must+be+between+1+and+20";

static const int default_stop_weight = 36;
static const int max_stop_weight = 100;
static const int min_stop_weight = 0;

static const int default_sleep_time = 15;
static const int max_sleep_time = 360;
static const int min_sleep_time = 0;

static const int default_refill_level = 3;
static const int max_refill_level = 20;
static const int min_refill_level = 1;

// room for up to this many bytes of enabled profile mask (128 profiles)
static const int max_profile_mask_bytes = 16;

enum class ConfigError {
  none = 0,
  invalid_sleep_time,
  invalid_stop_weight,
  invalid_refill_level,
};

class Config {
 public:
  Config()
      : m_sleepTime{default_sleep_time},
        m_stopWeight{default_stop_weight},
        m_refillLevel{default_refill_level},
        m_profile{profile_default},
        m_error(ConfigError::none) {
    resetEnabled();
    m_version = millis();
  }

  static Config fromQueryString(char* query) {
    auto stopWeight = getUnsignedInt(query, stop_weight_key);
    auto sleepTime = getUnsignedInt(query, sleep_time_key);
    auto refillLevel = getUnsignedInt(query, refill_level_key);
    auto profile = getUnsignedInt(query, profile_key);

    char enabled[max_profile_mask_bytes * 2 + 1] = "";
    getStringValue(query, enabled_key, enabled, sizeof(enabled));

    return Config(sleepTime, stopWeight, refillLevel, profile, enabled);
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

    const char* enabled = "";
    param = request->getParam(enabled_key, true, false);
    if (param) {
      enabled = param->value().c_str();
    }

    return Config(sleepTime, stopWeight, refillLevel, 0, enabled);
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
    }
    return buffer;
  }

  int getSleepTime() {
    return m_sleepTime;
  }

  int getStopWeight() {
    return m_stopWeight;
  }

  int getRefillLevel() {
    return m_refillLevel;
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
  Config(int sleepTime, int stopAtWeight, int refillLevel, int profile, const char* enabled)
      : m_sleepTime{sleepTime},
        m_stopWeight{stopAtWeight},
        m_refillLevel{refillLevel},
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
    auto value = parseUnsignedInt(buffer);

    return value;
  }

  // how long before we put the machine to sleep
  int m_sleepTime;

  // the weight we will stop at
  int m_stopWeight;

  // the water level (in mm) at which the machine asks for a refill
  int m_refillLevel;

  // the selected profile, 1-based index into the compiled in profiles
  int m_profile;

  // bitmask of which profiles are enabled for cycling
  uint8_t m_enabled[max_profile_mask_bytes];

  // the last error encountered while parsing
  ConfigError m_error;

  // the version for this config, used to detect changes by widgets
  unsigned long m_version;
};

Config readConfig() {
  auto config = Config();

  EEPROM.begin(EEPROM_SIZE);
  uint8_t length = EEPROM.read(0);
  if (length < 1 || length >= EEPROM_SIZE) {
    return config;
  }

  char buffer[EEPROM_SIZE + 1];
  auto read = EEPROM.readString(1, buffer, length);
  buffer[read] = 0;

  // set our config to the parsed value
  config = Config::fromQueryString(buffer);
  char query[EEPROM_SIZE - 2];
  config.toURLQuery(query, EEPROM_SIZE - 2);
  Serial.printf("eeprom: %s config: %s\n", buffer, query);

  // if there was an error reading it, reset to default
  if (config.getError() != ConfigError::none) {
    config = Config();
  }

  return config;
}

bool writeConfig(Config config) {
  char query[EEPROM_SIZE - 2];
  config.toURLQuery(query, EEPROM_SIZE - 2);

  // first write our length
  EEPROM.write(0, uint8_t(strlen(query)));

  // then our query string representation (including null byte)
  EEPROM.writeString(1, query);

  Serial.printf("wrote to EEPROM: %s\n", query);

  return EEPROM.commit();
}