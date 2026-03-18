#include "touch_xpt2046.h"

#include "delay.h"
#include "lcd_st7789v_16bit.h"

#define TP_PIN_CLK   GPIO_Pin_0   /* PE0  */
#define TP_PIN_CS    GPIO_Pin_13  /* PD13 */
#define TP_PIN_MOSI  GPIO_Pin_2   /* PE2  */
#define TP_PIN_MISO  GPIO_Pin_3   /* PE3  */
#define TP_PIN_INT   GPIO_Pin_4   /* PE4, active low */

#define TP_PORT_D GPIOD
#define TP_PORT_E GPIOE

#define TP_CMD_READ_X 0xD0u
#define TP_CMD_READ_Y 0x90u

/* Defaults before user calibration. */
#define TP_RAW_X_MIN_DEFAULT 120u
#define TP_RAW_X_MAX_DEFAULT 3600u
#define TP_RAW_Y_MIN_DEFAULT 120u
#define TP_RAW_Y_MAX_DEFAULT 3980u

/* Physical orientation options for this board. */
#define TP_SWAP_XY   1u
#define TP_INVERT_X  0u
#define TP_INVERT_Y  1u

#define TP_SAMPLE_COUNT      5u
#define TP_STABLE_THRESHOLD  420u
#define TP_RAW_MAX           4095u

static uint16_t g_raw_x_min = TP_RAW_X_MIN_DEFAULT;
static uint16_t g_raw_x_max = TP_RAW_X_MAX_DEFAULT;
static uint16_t g_raw_y_min = TP_RAW_Y_MIN_DEFAULT;
static uint16_t g_raw_y_max = TP_RAW_Y_MAX_DEFAULT;

static inline void TP_CS_L(void)   { GPIO_ResetBits(TP_PORT_D, TP_PIN_CS); }
static inline void TP_CS_H(void)   { GPIO_SetBits(TP_PORT_D, TP_PIN_CS); }
static inline void TP_CLK_L(void)  { GPIO_ResetBits(TP_PORT_E, TP_PIN_CLK); }
static inline void TP_CLK_H(void)  { GPIO_SetBits(TP_PORT_E, TP_PIN_CLK); }
static inline void TP_MOSI_L(void) { GPIO_ResetBits(TP_PORT_E, TP_PIN_MOSI); }
static inline void TP_MOSI_H(void) { GPIO_SetBits(TP_PORT_E, TP_PIN_MOSI); }

static inline uint8_t TP_MISO_Read(void)
{
    return (uint8_t)GPIO_ReadInputDataBit(TP_PORT_E, TP_PIN_MISO);
}

static void TP_ShortDelay(void)
{
    uint8_t i;

    for (i = 0u; i < 10u; i++) {
        __NOP();
    }
}

static void TP_WriteByte(uint8_t value)
{
    uint8_t i;

    for (i = 0u; i < 8u; i++) {
        if (value & 0x80u) {
            TP_MOSI_H();
        } else {
            TP_MOSI_L();
        }
        value <<= 1;

        TP_CLK_H();
        TP_ShortDelay();
        TP_CLK_L();
        TP_ShortDelay();
    }
}

static uint16_t TP_Read12Bit(uint8_t cmd)
{
    uint16_t value = 0u;
    uint8_t i;

    TP_CS_L();
    TP_WriteByte(cmd);

    for (i = 0u; i < 16u; i++) {
        value <<= 1;
        TP_CLK_H();
        TP_ShortDelay();
        if (TP_MISO_Read() != 0u) {
            value |= 1u;
        }
        TP_CLK_L();
        TP_ShortDelay();
    }

    TP_CS_H();
    return (uint16_t)((value >> 4) & 0x0FFFu);
}

static uint8_t TP_ReadStable(uint8_t cmd, uint16_t *out)
{
    uint16_t v[TP_SAMPLE_COUNT];
    uint16_t t;
    uint8_t i;
    uint8_t j;
    uint32_t sum = 0u;

    if (out == 0) {
        return 0u;
    }

    for (i = 0u; i < TP_SAMPLE_COUNT; i++) {
        if (TOUCH_IsPressed() == 0u) {
            return 0u;
        }
        v[i] = TP_Read12Bit(cmd);
    }

    for (i = 0u; i < (TP_SAMPLE_COUNT - 1u); i++) {
        for (j = 0u; j < (uint8_t)(TP_SAMPLE_COUNT - 1u - i); j++) {
            if (v[j] > v[j + 1u]) {
                t = v[j];
                v[j] = v[j + 1u];
                v[j + 1u] = t;
            }
        }
    }

    if ((uint16_t)(v[TP_SAMPLE_COUNT - 2u] - v[1u]) > TP_STABLE_THRESHOLD) {
        *out = v[TP_SAMPLE_COUNT / 2u];
        return 1u;
    }

    for (i = 1u; i < (TP_SAMPLE_COUNT - 1u); i++) {
        sum += v[i];
    }

    *out = (uint16_t)(sum / (TP_SAMPLE_COUNT - 2u));
    return 1u;
}

static void TP_AlignRaw(uint16_t raw_x, uint16_t raw_y, uint16_t *aligned_x, uint16_t *aligned_y)
{
    uint16_t x = raw_x;
    uint16_t y = raw_y;

#if TP_SWAP_XY
    {
        uint16_t t = x;
        x = y;
        y = t;
    }
#endif

#if TP_INVERT_X
    x = (uint16_t)(TP_RAW_MAX - x);
#endif
#if TP_INVERT_Y
    y = (uint16_t)(TP_RAW_MAX - y);
#endif

    *aligned_x = x;
    *aligned_y = y;
}

static uint16_t TP_MapRaw(uint16_t raw, uint16_t in_a, uint16_t in_b, uint16_t out_max)
{
    uint32_t num;
    uint32_t den;

    if (in_a == in_b) {
        return (uint16_t)(out_max / 2u);
    }

    if (in_b > in_a) {
        if (raw <= in_a) {
            return 0u;
        }
        if (raw >= in_b) {
            return out_max;
        }

        num = (uint32_t)(raw - in_a) * (uint32_t)out_max;
        den = (uint32_t)(in_b - in_a);
    } else {
        if (raw >= in_a) {
            return 0u;
        }
        if (raw <= in_b) {
            return out_max;
        }

        num = (uint32_t)(in_a - raw) * (uint32_t)out_max;
        den = (uint32_t)(in_a - in_b);
    }

    return (uint16_t)(num / den);
}

void TOUCH_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE, ENABLE);

    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;

    gpio.GPIO_Pin = TP_PIN_CS;
    GPIO_Init(TP_PORT_D, &gpio);

    gpio.GPIO_Pin = TP_PIN_CLK | TP_PIN_MOSI;
    GPIO_Init(TP_PORT_E, &gpio);

    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Pin = TP_PIN_MISO | TP_PIN_INT;
    GPIO_Init(TP_PORT_E, &gpio);

    TP_CS_H();
    TP_CLK_L();
    TP_MOSI_H();
}

uint8_t TOUCH_IsPressed(void)
{
    return (GPIO_ReadInputDataBit(TP_PORT_E, TP_PIN_INT) == Bit_RESET) ? 1u : 0u;
}

uint8_t TOUCH_GetRawPoint(uint16_t *raw_x, uint16_t *raw_y)
{
    uint16_t x;
    uint16_t y;
    uint8_t retry;

    if ((raw_x == 0) || (raw_y == 0)) {
        return 0u;
    }

    if (TOUCH_IsPressed() == 0u) {
        return 0u;
    }

    for (retry = 0u; retry < 5u; retry++) {
        if ((TP_ReadStable(TP_CMD_READ_X, &x) != 0u) && (TP_ReadStable(TP_CMD_READ_Y, &y) != 0u)) {
            TP_AlignRaw(x, y, raw_x, raw_y);
            return 1u;
        }

        Delay_Ms(1u);
        if (TOUCH_IsPressed() == 0u) {
            break;
        }
    }

    return 0u;
}

uint8_t TOUCH_GetPoint(uint16_t *x, uint16_t *y)
{
    uint16_t raw_x;
    uint16_t raw_y;

    if ((x == 0) || (y == 0)) {
        return 0u;
    }

    if (TOUCH_GetRawPoint(&raw_x, &raw_y) == 0u) {
        return 0u;
    }

    *x = TP_MapRaw(raw_x, g_raw_x_min, g_raw_x_max, (uint16_t)(LCD_WIDTH - 1u));
    *y = TP_MapRaw(raw_y, g_raw_y_min, g_raw_y_max, (uint16_t)(LCD_HEIGHT - 1u));
    return 1u;
}

void TOUCH_SetCalibration(uint16_t raw_x_min, uint16_t raw_x_max, uint16_t raw_y_min, uint16_t raw_y_max)
{
    g_raw_x_min = raw_x_min;
    g_raw_x_max = raw_x_max;
    g_raw_y_min = raw_y_min;
    g_raw_y_max = raw_y_max;
}

void TOUCH_GetCalibration(uint16_t *raw_x_min, uint16_t *raw_x_max, uint16_t *raw_y_min, uint16_t *raw_y_max)
{
    if (raw_x_min != 0) {
        *raw_x_min = g_raw_x_min;
    }
    if (raw_x_max != 0) {
        *raw_x_max = g_raw_x_max;
    }
    if (raw_y_min != 0) {
        *raw_y_min = g_raw_y_min;
    }
    if (raw_y_max != 0) {
        *raw_y_max = g_raw_y_max;
    }
}