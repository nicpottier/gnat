#pragma once

// file backed eeprom so config persists across emulator runs

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

class EEPROMClass {
 public:
  void begin(int size) {
    m_data.assign(size, 0xFF);
    auto f = fopen(path(), "rb");
    if (f) {
      auto read = fread(m_data.data(), 1, m_data.size(), f);
      (void)read;
      fclose(f);
    }
  }

  uint8_t read(int addr) {
    return inBounds(addr) ? m_data[addr] : 0xFF;
  }

  void write(int addr, uint8_t value) {
    if (inBounds(addr)) {
      m_data[addr] = value;
    }
  }

  void writeString(int addr, const char* s) {
    for (int i = 0; s[i] && inBounds(addr + i); i++) {
      m_data[addr + i] = s[i];
    }
    if (inBounds(addr + (int)strlen(s))) {
      m_data[addr + strlen(s)] = 0;
    }
  }

  size_t readString(int addr, char* buffer, int length) {
    int i = 0;
    for (; i < length && inBounds(addr + i) && m_data[addr + i]; i++) {
      buffer[i] = m_data[addr + i];
    }
    return i;
  }

  bool commit() {
    auto f = fopen(path(), "wb");
    if (!f) {
      return false;
    }
    fwrite(m_data.data(), 1, m_data.size(), f);
    fclose(f);
    return true;
  }

 private:
  const char* path() {
    return "gnat-emu.eeprom";
  }

  bool inBounds(int addr) {
    return addr >= 0 && addr < (int)m_data.size();
  }

  std::vector<uint8_t> m_data;
};

inline EEPROMClass EEPROM;
