#pragma once

// minimal fake of the esp-idf spi_master API, enough to compile and exercise
// src/amoled/rm67162.cpp on the desktop. spi_device_polling_transmit records
// the panel command stream so tests can inspect what the driver actually
// sends the display.

#include <cstdint>
#include <cstring>
#include <vector>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERROR_CHECK(x) (void)(x)

#define SPI2_HOST 1
#define SPI_DMA_CH_AUTO 0

#define SPI_TRANS_MULTILINE_CMD (1 << 0)
#define SPI_TRANS_MULTILINE_ADDR (1 << 1)
#define SPI_TRANS_MODE_QIO (1 << 2)
#define SPI_TRANS_VARIABLE_CMD (1 << 3)
#define SPI_TRANS_VARIABLE_ADDR (1 << 4)
#define SPI_TRANS_VARIABLE_DUMMY (1 << 5)
#define SPI_TRANS_MODE_DIOQIO_ADDR (1 << 6)
#define SPICOMMON_BUSFLAG_MASTER (1 << 0)
#define SPICOMMON_BUSFLAG_GPIO_PINS (1 << 1)
#define SPICOMMON_BUSFLAG_QUAD (1 << 2)
#define SPI_DEVICE_HALFDUPLEX (1 << 0)

typedef void* spi_device_handle_t;

typedef struct {
  uint32_t flags;
  uint16_t cmd;
  uint64_t addr;
  size_t length;
  size_t rxlength;
  void* user;
  const void* tx_buffer;
  void* rx_buffer;
} spi_transaction_t;

typedef struct {
  spi_transaction_t base;
  uint8_t command_bits;
  uint8_t address_bits;
  uint8_t dummy_bits;
} spi_transaction_ext_t;

// field order matches the designated initializers in rm67162.cpp
typedef struct {
  int data0_io_num;
  int data1_io_num;
  int sclk_io_num;
  int data2_io_num;
  int data3_io_num;
  int max_transfer_sz;
  uint32_t flags;
} spi_bus_config_t;

typedef struct {
  uint8_t command_bits;
  uint8_t address_bits;
  int mode;
  int clock_speed_hz;
  int spics_io_num;
  uint32_t flags;
  int queue_size;
} spi_device_interface_config_t;

// a single panel command as seen on the wire: the command byte plus its data
struct FakeSpiCommand {
  uint8_t cmd;
  std::vector<uint8_t> data;
};

inline std::vector<FakeSpiCommand>& fakeSpiLog() {
  static std::vector<FakeSpiCommand> log;
  return log;
}

inline esp_err_t spi_bus_initialize(int, const spi_bus_config_t*, int) { return ESP_OK; }

inline esp_err_t spi_bus_add_device(int, const spi_device_interface_config_t*, spi_device_handle_t* out) {
  static int dummy;
  *out = &dummy;
  return ESP_OK;
}

inline esp_err_t spi_device_polling_transmit(spi_device_handle_t, spi_transaction_t* t) {
  // rm67162 sends single commands with t.cmd == 0x02 and the panel command
  // in the top byte of the address; pixel pushes use 0x32/0x12, which carry
  // no panel command and we ignore
  if (t->cmd == 0x02) {
    FakeSpiCommand c;
    c.cmd = uint8_t((t->addr >> 8) & 0xFF);
    size_t nbytes = t->length / 8;
    auto src = (const uint8_t*)t->tx_buffer;
    for (size_t i = 0; i < nbytes && src; i++) {
      c.data.push_back(src[i]);
    }
    fakeSpiLog().push_back(std::move(c));
  }
  return ESP_OK;
}
