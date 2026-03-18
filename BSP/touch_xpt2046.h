#ifndef TOUCH_XPT2046_H
#define TOUCH_XPT2046_H

#include "stm32f10x.h"
#include <stdint.h>

void TOUCH_Init(void);
uint8_t TOUCH_IsPressed(void);
uint8_t TOUCH_GetRawPoint(uint16_t *raw_x, uint16_t *raw_y);
uint8_t TOUCH_GetPoint(uint16_t *x, uint16_t *y);
void TOUCH_SetCalibration(uint16_t raw_x_min, uint16_t raw_x_max, uint16_t raw_y_min, uint16_t raw_y_max);
void TOUCH_GetCalibration(uint16_t *raw_x_min, uint16_t *raw_x_max, uint16_t *raw_y_min, uint16_t *raw_y_max);

#endif