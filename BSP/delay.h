#ifndef DELAY_H
#define DELAY_H

#include "stm32f10x.h"
#include <stdint.h>

void Delay_Init(uint32_t sysclk_hz);
void Delay_TickInc(void);
void Delay_Ms(uint32_t ms);

#endif
