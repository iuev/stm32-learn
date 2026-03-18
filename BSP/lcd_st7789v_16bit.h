#ifndef LCD_ST7789V_16BIT_H
#define LCD_ST7789V_16BIT_H

#include "stm32f10x.h"
#include <stdint.h>

#define LCD_WIDTH  240u
#define LCD_HEIGHT 320u

#define LCD_COLOR_BLACK 0x0000u
#define LCD_COLOR_WHITE 0xFFFFu
#define LCD_COLOR_RED   0xF800u
#define LCD_COLOR_GREEN 0x07E0u
#define LCD_COLOR_BLUE  0x001Fu
#define LCD_COLOR_YELLOW 0xFFE0u

void LCD_Init(void);
void LCD_Clear(uint16_t color);
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_DrawChar5x7(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale);
void LCD_DrawString(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale);

#endif
