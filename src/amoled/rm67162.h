// driver for the RM67162 AMOLED panel on the LilyGO T-Display S3 AMOLED,
// adapted from Xinyuan-LilyGO/T-Display-S3-AMOLED examples
#pragma once

#include "stdint.h"

void rm67162_init(void);
void lcd_setRotation(uint8_t r);
void lcd_address_set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void lcd_fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color);
void lcd_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void lcd_PushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t high, uint16_t *data);
void lcd_PushColors(uint16_t *data, uint32_t len);
void lcd_sleep();
void lcd_wake();
