#include "delay.h"

static volatile uint32_t s_ms_tick = 0;

void Delay_TickInc(void)
{
    s_ms_tick++;
}

void Delay_Init(uint32_t sysclk_hz)
{
    SysTick_Config(sysclk_hz / 1000u);
}

void Delay_Ms(uint32_t ms)
{
    uint32_t end = s_ms_tick + ms;
    while ((int32_t)(end - s_ms_tick) > 0) {
    }
}
