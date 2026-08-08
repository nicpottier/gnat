// minimal polling driver for the CST816 capacitive touch controller
#pragma once

#include <Wire.h>

const uint8_t cst816_addr = 0x15;

class CST816 {
 public:
  bool begin(int sda, int scl) {
    Wire.begin(sda, scl);
    Wire.beginTransmission(cst816_addr);
    if (Wire.endTransmission() != 0) {
      return false;
    }

    // disable auto sleep so polling keeps working without the interrupt pin
    Wire.beginTransmission(cst816_addr);
    Wire.write(0xFE);
    Wire.write(0x01);
    Wire.endTransmission();
    return true;
  }

  // reads the current touch point, returns true while touched
  bool read(int& x, int& y) {
    uint8_t data[6];
    Wire.beginTransmission(cst816_addr);
    Wire.write(0x01);
    if (Wire.endTransmission(false) != 0) {
      return false;
    }
    if (Wire.requestFrom(int(cst816_addr), 6) != 6) {
      return false;
    }
    for (int i = 0; i < 6; i++) {
      data[i] = Wire.read();
    }

    // gesture, fingers, then 12 bit x and y
    if (data[1] == 0) {
      return false;
    }
    x = ((data[2] & 0x0F) << 8) | data[3];
    y = ((data[4] & 0x0F) << 8) | data[5];
    return true;
  }
};
