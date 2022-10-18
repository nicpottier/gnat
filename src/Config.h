#pragma once
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>

#define EEPROM_SIZE 128

static const char* stop_weight_key = "stop_weight";
static const char* sleep_time_key = "sleep_time";
static const char* error_key = "error";

static const char* invalid_sleep_time_error = "Invalid+sleep+time,+must+be+less+than+360";
static const char* invalid_stop_weight_error = "Invalid+stop+weight,+must+be+less+than+100";

static const int default_stop_weight = 36;
static const int default_sleep_time = 15;

enum class ConfigError {
  none = 0,
  invalid_sleep_time,
  invalid_stop_weight,
};

class Config {
 public:
  Config()
      : m_sleepTime{0},
        m_stopWeight{0},
        m_error(ConfigError::none) {}

  static Config fromQueryString(char* query) {
    auto stopWeight = getUnsignedInt(query, stop_weight_key);
    auto sleepTime = getUnsignedInt(query, sleep_time_key);
    return Config(sleepTime, stopWeight);
  }

  static Config fromRequest(AsyncWebServerRequest* request) {
    int sleepTime = 0;
    int stopWeight = 0;

    auto param = request->getParam(sleep_time_key, true, false);
    if (param) {
      sleepTime = parseUnsignedInt(param->value().c_str());
    }

    param = request->getParam(stop_weight_key, true, false);
    if (param) {
      stopWeight = parseUnsignedInt(param->value().c_str());
    }

    return Config(sleepTime, stopWeight);
  }

  // returns a url encoded version of the config, suitable for writing to EEProm
  const char* toURLQuery(char* buffer, int size) {
    char* field = buffer;

    if (m_stopWeight != 0) {
      field += snprintf(field, size, "%s=%d&", stop_weight_key, m_stopWeight);
    }
    if (m_sleepTime != 0) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%d", sleep_time_key, m_sleepTime);
    }
    if (m_error == ConfigError::invalid_sleep_time) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%s", error_key, invalid_sleep_time_error);
    } else if (m_error == ConfigError::invalid_stop_weight) {
      size -= strlen(field);
      field += snprintf(field, size, "%s=%s", error_key, invalid_stop_weight_error);
    }
    return buffer;
  }

  int getSleepTime() {
    return m_sleepTime;
  }

  int getStopWeight() {
    return m_stopWeight;
  }

  ConfigError getError() {
    return m_error;
  }

 private:
  Config(int sleepTime, int stopAtWeight)
      : m_sleepTime{sleepTime},
        m_stopWeight{stopAtWeight},
        m_error{ConfigError::none} {
    if (m_sleepTime == 0) {
      m_sleepTime = default_sleep_time;
    }

    else if (m_sleepTime < 0 || m_sleepTime > 360) {
      m_error = ConfigError::invalid_sleep_time;
    }

    if (m_stopWeight == 0) {
      m_stopWeight = default_stop_weight;
    } else if (m_stopWeight < 0 || m_stopWeight > 100) {
      m_error = ConfigError::invalid_stop_weight;
    }
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

  int m_sleepTime;
  int m_stopWeight;
  ConfigError m_error;
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