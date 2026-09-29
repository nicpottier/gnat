// minimal polling driver for the CST226SE capacitive touch controller
#pragma once

#include <Wire.h>

const uint8_t cst226_addr = 0x5A;

class CST226 {
 public:
  bool begin(int sda, int scl, int rst) {
    // the controller only answers after a reset pulse
    if (rst >= 0) {
      pinMode(rst, OUTPUT);
      digitalWrite(rst, LOW);
      delay(100);
      digitalWrite(rst, HIGH);
      delay(100);
    }

    Wire.begin(sda, scl);
    Wire.beginTransmission(cst226_addr);
    return Wire.endTransmission() == 0;
  }

  // reads the first touch point in panel coordinates, returns true while touched
  bool read(int& x, int& y) {
    uint8_t data[28];
    Wire.beginTransmission(cst226_addr);
    Wire.write(0x00);
    if (Wire.endTransmission(false) != 0) {
      return false;
    }
    if (Wire.requestFrom(int(cst226_addr), 28) != 28) {
      return false;
    }
    for (int i = 0; i < 28; i++) {
      data[i] = Wire.read();
    }

    // byte 6 is a fixed 0xAB marker on valid reports
    if (data[0] == 0xAB || data[6] != 0xAB) {
      return false;
    }

    // bad point counts need acknowledging before the controller reports again
    int points = data[5] & 0x7F;
    if (points == 0 || points > 5) {
      Wire.beginTransmission(cst226_addr);
      Wire.write(0x00);
      Wire.write(0xAB);
      Wire.endTransmission();
      return false;
    }

    // 12 bit x and y, the low nibbles packed together in byte 3
    x = (data[1] << 4) | ((data[3] >> 4) & 0x0F);
    y = (data[2] << 4) | (data[3] & 0x0F);
    return true;
  }
};
