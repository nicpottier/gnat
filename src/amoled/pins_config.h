// pin configuration for the LilyGO T-Display S3 AMOLED (RM67162 panel),
// from Xinyuan-LilyGO/T-Display-S3-AMOLED examples
#pragma once

#define SPI_FREQUENCY  75000000
#define TFT_SPI_MODE   SPI_MODE0
#define TFT_SPI_HOST   SPI2_HOST

#define SEND_BUF_SIZE  (0x4000)

#define TFT_TE         9
#define TFT_SDO        8
#define TFT_DC         7
#define TFT_RES        17
#define TFT_CS         6
#define TFT_MOSI       18
#define TFT_SCK        47

#define TFT_QSPI_CS    6
#define TFT_QSPI_SCK   47
#define TFT_QSPI_D0    18
#define TFT_QSPI_D1    7
#define TFT_QSPI_D2    48
#define TFT_QSPI_D3    5
#define TFT_QSPI_RST   17

// the panel's power IC is gated behind this pin, it must be driven high
#define PIN_PMIC_EN    38
