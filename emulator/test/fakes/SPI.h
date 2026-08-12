#pragma once

// minimal Arduino SPI stand-in. the amoled build uses the QSPI path (through
// driver/spi_master.h), so this only needs to satisfy the compiler for the
// bit-banged fallback that isn't exercised here.

#include <cstdint>

#define MSBFIRST 1
#define SPI_MODE0 0

class SPISettings {
 public:
  SPISettings(uint32_t, uint8_t, uint8_t) {}
};

class SPIClass {
 public:
  void begin(int = -1, int = -1, int = -1, int = -1) {}
  void setFrequency(uint32_t) {}
  void beginTransaction(SPISettings) {}
  void endTransaction() {}
  void write(uint8_t) {}
  void write16(uint16_t) {}
  void writeBytes(uint8_t*, uint32_t) {}
};
inline SPIClass SPI;
