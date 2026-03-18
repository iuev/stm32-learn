#include "stm32f10x.h"

#include "delay.h"
#include "lcd_st7789v_16bit.h"
#include "touch_xpt2046.h"

#define BTN_Y      90u
#define BTN_W      80u
#define BTN_H      120u
#define BTN1_X     20u
#define BTN2_X     140u

#define HIT_PAD      20u
#define BTN_BAND_TOP ((uint16_t)(BTN_Y - HIT_PAD))
#define BTN_BAND_BTM ((uint16_t)(BTN_Y + BTN_H + HIT_PAD))

#define CAL_PT_MARGIN        20u
#define CAL_WAIT_MS          6u
#define CAL_BOOT_WINDOW_MS   3000u
#define CAL_BOOT_HOLD_MS     1000u
#define CAL_RUN_HOLD_MS      1000u
#define CAL_BTN_X            176u
#define CAL_BTN_Y            4u
#define CAL_BTN_W            60u
#define CAL_BTN_H            30u

typedef struct
{
    uint16_t x;
    uint16_t y;
} TouchRawPoint;

static TouchRawPoint g_cal_lt;
static TouchRawPoint g_cal_rt;
static TouchRawPoint g_cal_lb;
static TouchRawPoint g_cal_rb;

static void LED_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);

    /* Board schematic: LED anode -> 3V3, PBx low level turns LED on. */
    GPIO_SetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1);
}

static void LED_Set(uint8_t pb0_on, uint8_t pb1_on)
{
    if (pb0_on != 0u) {
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);
    } else {
        GPIO_SetBits(GPIOB, GPIO_Pin_0);
    }

    if (pb1_on != 0u) {
        GPIO_ResetBits(GPIOB, GPIO_Pin_1);
    } else {
        GPIO_SetBits(GPIOB, GPIO_Pin_1);
    }
}

static uint8_t InRect(uint16_t x, uint16_t y, uint16_t rx, uint16_t ry, uint16_t rw, uint16_t rh)
{
    if ((x < rx) || (x >= (uint16_t)(rx + rw))) {
        return 0u;
    }
    if ((y < ry) || (y >= (uint16_t)(ry + rh))) {
        return 0u;
    }
    return 1u;
}

static uint16_t MapRawAxis(uint16_t raw, uint16_t in_a, uint16_t in_b, uint16_t out_max)
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

static void SetDefaultCalPoints(void)
{
    uint16_t x_min;
    uint16_t x_max;
    uint16_t y_min;
    uint16_t y_max;

    TOUCH_GetCalibration(&x_min, &x_max, &y_min, &y_max);

    g_cal_lt.x = x_min;
    g_cal_lt.y = y_min;
    g_cal_rt.x = x_max;
    g_cal_rt.y = y_min;
    g_cal_lb.x = x_min;
    g_cal_lb.y = y_max;
    g_cal_rb.x = x_max;
    g_cal_rb.y = y_max;
}

static uint8_t TouchRawToPixel(uint16_t raw_x, uint16_t raw_y, uint16_t *x, uint16_t *y)
{
    uint16_t x_min;
    uint16_t x_max;
    uint16_t y_min;
    uint16_t y_max;

    if ((x == 0) || (y == 0)) {
        return 0u;
    }

    TOUCH_GetCalibration(&x_min, &x_max, &y_min, &y_max);

    *x = MapRawAxis(raw_x, x_min, x_max, (uint16_t)(LCD_WIDTH - 1u));
    *y = MapRawAxis(raw_y, y_min, y_max, (uint16_t)(LCD_HEIGHT - 1u));
    return 1u;
}

static int32_t Dot2(int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    return ax * bx + ay * by;
}

static uint8_t TouchRawToNorm1000(uint16_t raw_x, uint16_t raw_y, int32_t *u1000, int32_t *v1000)
{
    int32_t left_x;
    int32_t left_y;
    int32_t right_x;
    int32_t right_y;
    int32_t top_x;
    int32_t top_y;
    int32_t bot_x;
    int32_t bot_y;
    int32_t hx;
    int32_t hy;
    int32_t vx;
    int32_t vy;
    int32_t px;
    int32_t py;
    int32_t hden;
    int32_t vden;

    if ((u1000 == 0) || (v1000 == 0)) {
        return 0u;
    }

    left_x = ((int32_t)g_cal_lt.x + (int32_t)g_cal_lb.x) / 2;
    left_y = ((int32_t)g_cal_lt.y + (int32_t)g_cal_lb.y) / 2;
    right_x = ((int32_t)g_cal_rt.x + (int32_t)g_cal_rb.x) / 2;
    right_y = ((int32_t)g_cal_rt.y + (int32_t)g_cal_rb.y) / 2;

    top_x = ((int32_t)g_cal_lt.x + (int32_t)g_cal_rt.x) / 2;
    top_y = ((int32_t)g_cal_lt.y + (int32_t)g_cal_rt.y) / 2;
    bot_x = ((int32_t)g_cal_lb.x + (int32_t)g_cal_rb.x) / 2;
    bot_y = ((int32_t)g_cal_lb.y + (int32_t)g_cal_rb.y) / 2;

    hx = right_x - left_x;
    hy = right_y - left_y;
    vx = bot_x - top_x;
    vy = bot_y - top_y;

    hden = Dot2(hx, hy, hx, hy);
    vden = Dot2(vx, vy, vx, vy);
    if ((hden < 10) || (vden < 10)) {
        return 0u;
    }

    px = (int32_t)raw_x;
    py = (int32_t)raw_y;

    *u1000 = (Dot2(px - left_x, py - left_y, hx, hy) * 1000) / hden;
    *v1000 = (Dot2(px - top_x, py - top_y, vx, vy) * 1000) / vden;
    return 1u;
}

static uint8_t TouchRawInButtonBand(uint16_t raw_x, uint16_t raw_y, uint8_t *is_left)
{
    int32_t u1000;
    int32_t v1000;
    int32_t v_low;
    int32_t v_high;

    if (is_left == 0) {
        return 0u;
    }

    if (TouchRawToNorm1000(raw_x, raw_y, &u1000, &v1000) == 0u) {
        return 0u;
    }

    v_low = ((int32_t)BTN_BAND_TOP * 1000) / (int32_t)(LCD_HEIGHT - 1u);
    v_high = ((int32_t)BTN_BAND_BTM * 1000) / (int32_t)(LCD_HEIGHT - 1u);

    if ((v1000 < (v_low - 120)) || (v1000 > (v_high + 120))) {
        return 0u;
    }

    *is_left = (u1000 < 500) ? 1u : 0u;
    return 1u;
}

static void UI_DrawButton(uint16_t x, uint16_t y, uint16_t w, uint16_t h, char number)
{
    char text[2];

    LCD_FillRect(x, y, w, h, LCD_COLOR_YELLOW);
    LCD_FillRect((uint16_t)(x + 3u), (uint16_t)(y + 3u), (uint16_t)(w - 6u), (uint16_t)(h - 6u), LCD_COLOR_WHITE);

    text[0] = number;
    text[1] = '\0';
    LCD_DrawString((uint16_t)(x + 30u), (uint16_t)(y + 38u), text, LCD_COLOR_RED, LCD_COLOR_WHITE, 6u);
}

static void UI_DrawCalButton(void)
{
    LCD_FillRect(CAL_BTN_X, CAL_BTN_Y, CAL_BTN_W, CAL_BTN_H, LCD_COLOR_RED);
    LCD_DrawString((uint16_t)(CAL_BTN_X + 8u), (uint16_t)(CAL_BTN_Y + 8u), "CAL", LCD_COLOR_WHITE, LCD_COLOR_RED, 2u);
}

static void UI_ShowState(uint8_t pb0_on, uint8_t pb1_on)
{
    LCD_FillRect(0u, 250u, LCD_WIDTH, 70u, LCD_COLOR_BLUE);

    if (pb0_on != 0u) {
        LCD_DrawString(10u, 260u, "PB0 LED: ON ", LCD_COLOR_GREEN, LCD_COLOR_BLUE, 3u);
    } else {
        LCD_DrawString(10u, 260u, "PB0 LED: OFF", LCD_COLOR_WHITE, LCD_COLOR_BLUE, 3u);
    }

    if (pb1_on != 0u) {
        LCD_DrawString(10u, 285u, "PB1 LED: ON ", LCD_COLOR_GREEN, LCD_COLOR_BLUE, 3u);
    } else {
        LCD_DrawString(10u, 285u, "PB1 LED: OFF", LCD_COLOR_WHITE, LCD_COLOR_BLUE, 3u);
    }
}

static void UI_DrawMain(void)
{
    LCD_Clear(LCD_COLOR_BLUE);
    LCD_DrawString(12u, 20u, "Touch 1 -> Toggle PB0", LCD_COLOR_WHITE, LCD_COLOR_BLUE, 2u);
    LCD_DrawString(12u, 45u, "Touch 2 -> Toggle PB1", LCD_COLOR_WHITE, LCD_COLOR_BLUE, 2u);
    LCD_DrawString(12u, 68u, "Hold CAL to calibrate", LCD_COLOR_YELLOW, LCD_COLOR_BLUE, 2u);

    UI_DrawCalButton();
    UI_DrawButton(BTN1_X, BTN_Y, BTN_W, BTN_H, '1');
    UI_DrawButton(BTN2_X, BTN_Y, BTN_W, BTN_H, '2');
}

static void WaitTouchRelease(void)
{
    while (TOUCH_IsPressed() != 0u) {
        Delay_Ms(CAL_WAIT_MS);
    }
}

static void UI_DrawCross(uint16_t cx, uint16_t cy, uint16_t color)
{
    LCD_FillRect((uint16_t)(cx - 1u), (uint16_t)(cy - 12u), 3u, 25u, color);
    LCD_FillRect((uint16_t)(cx - 12u), (uint16_t)(cy - 1u), 25u, 3u, color);
}

static uint16_t U16AbsDiff(uint16_t a, uint16_t b)
{
    return (a > b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static uint8_t CaptureCalibrationPoint(uint16_t scr_x, uint16_t scr_y, const char *label, TouchRawPoint *out)
{
    uint32_t sum_x;
    uint32_t sum_y;
    uint8_t count;
    uint8_t i;
    uint16_t raw_x;
    uint16_t raw_y;

    if (out == 0) {
        return 0u;
    }

    LCD_Clear(LCD_COLOR_BLUE);
    LCD_DrawString(12u, 20u, "CALIBRATION", LCD_COLOR_WHITE, LCD_COLOR_BLUE, 3u);
    LCD_DrawString(12u, 55u, "Touch target:", LCD_COLOR_WHITE, LCD_COLOR_BLUE, 2u);
    LCD_DrawString(12u, 80u, label, LCD_COLOR_YELLOW, LCD_COLOR_BLUE, 3u);
    LCD_DrawString(12u, 115u, "Keep finger steady", LCD_COLOR_WHITE, LCD_COLOR_BLUE, 2u);
    UI_DrawCross(scr_x, scr_y, LCD_COLOR_RED);

    WaitTouchRelease();

    while (1) {
        if (TOUCH_IsPressed() != 0u) {
            Delay_Ms(35u);

            sum_x = 0u;
            sum_y = 0u;
            count = 0u;

            for (i = 0u; i < 8u; i++) {
                if (TOUCH_GetRawPoint(&raw_x, &raw_y) != 0u) {
                    sum_x += raw_x;
                    sum_y += raw_y;
                    count++;
                }
                Delay_Ms(5u);
            }

            if (count >= 3u) {
                out->x = (uint16_t)(sum_x / count);
                out->y = (uint16_t)(sum_y / count);
                UI_DrawCross(scr_x, scr_y, LCD_COLOR_GREEN);
                WaitTouchRelease();
                Delay_Ms(120u);
                return 1u;
            }

            WaitTouchRelease();
        }

        Delay_Ms(CAL_WAIT_MS);
    }
}

static uint8_t RunCalibration(void)
{
    TouchRawPoint lt;
    TouchRawPoint rt;
    TouchRawPoint lb;
    TouchRawPoint rb;
    uint16_t x_min;
    uint16_t x_max;
    uint16_t y_min;
    uint16_t y_max;

    if (CaptureCalibrationPoint(CAL_PT_MARGIN, CAL_PT_MARGIN, "LEFT TOP", &lt) == 0u) {
        return 0u;
    }
    if (CaptureCalibrationPoint((uint16_t)(LCD_WIDTH - 1u - CAL_PT_MARGIN), CAL_PT_MARGIN, "RIGHT TOP", &rt) == 0u) {
        return 0u;
    }
    if (CaptureCalibrationPoint(CAL_PT_MARGIN, (uint16_t)(LCD_HEIGHT - 1u - CAL_PT_MARGIN), "LEFT BOTTOM", &lb) == 0u) {
        return 0u;
    }
    if (CaptureCalibrationPoint((uint16_t)(LCD_WIDTH - 1u - CAL_PT_MARGIN), (uint16_t)(LCD_HEIGHT - 1u - CAL_PT_MARGIN), "RIGHT BOTTOM", &rb) == 0u) {
        return 0u;
    }

    x_min = (uint16_t)(((uint32_t)lt.x + (uint32_t)lb.x) / 2u);
    x_max = (uint16_t)(((uint32_t)rt.x + (uint32_t)rb.x) / 2u);
    y_min = (uint16_t)(((uint32_t)lt.y + (uint32_t)rt.y) / 2u);
    y_max = (uint16_t)(((uint32_t)lb.y + (uint32_t)rb.y) / 2u);

    if ((U16AbsDiff(x_min, x_max) < 900u) || (U16AbsDiff(y_min, y_max) < 900u)) {
        LCD_Clear(LCD_COLOR_RED);
        LCD_DrawString(20u, 130u, "CAL FAIL", LCD_COLOR_WHITE, LCD_COLOR_RED, 4u);
        Delay_Ms(700u);
        return 0u;
    }

    TOUCH_SetCalibration(x_min, x_max, y_min, y_max);

    g_cal_lt = lt;
    g_cal_rt = rt;
    g_cal_lb = lb;
    g_cal_rb = rb;

    LCD_Clear(LCD_COLOR_GREEN);
    LCD_DrawString(20u, 130u, "CAL OK", LCD_COLOR_BLACK, LCD_COLOR_GREEN, 4u);
    Delay_Ms(500u);
    return 1u;
}

static uint8_t NeedCalibrationAtBoot(void)
{
    uint16_t elapsed = 0u;
    uint16_t hold_ms = 0u;

    LCD_Clear(LCD_COLOR_BLUE);
    LCD_DrawString(16u, 100u, "Hold touch to CAL", LCD_COLOR_WHITE, LCD_COLOR_BLUE, 2u);
    LCD_DrawString(16u, 125u, "within 3s after boot", LCD_COLOR_WHITE, LCD_COLOR_BLUE, 2u);

    while (elapsed < CAL_BOOT_WINDOW_MS) {
        if (TOUCH_IsPressed() != 0u) {
            hold_ms = (uint16_t)(hold_ms + CAL_WAIT_MS);
            if (hold_ms >= CAL_BOOT_HOLD_MS) {
                WaitTouchRelease();
                return 1u;
            }
        } else {
            hold_ms = 0u;
        }

        Delay_Ms(CAL_WAIT_MS);
        elapsed = (uint16_t)(elapsed + CAL_WAIT_MS);
    }

    return 0u;
}

int main(void)
{
    uint16_t x;
    uint16_t y;
    uint16_t raw_x;
    uint16_t raw_y;
    uint16_t hold_ms;
    uint8_t is_left;
    uint8_t pb0_on = 0u;
    uint8_t pb1_on = 0u;

    SystemCoreClockUpdate();
    Delay_Init(SystemCoreClock);

    LED_Init();
    TOUCH_Init();
    LCD_Init();

    SetDefaultCalPoints();

    if (NeedCalibrationAtBoot() != 0u) {
        RunCalibration();
    }

    UI_DrawMain();
    UI_ShowState(pb0_on, pb1_on);

    while (1) {
        if (TOUCH_GetRawPoint(&raw_x, &raw_y) != 0u) {
            TouchRawToPixel(raw_x, raw_y, &x, &y);

            if (InRect(x, y, CAL_BTN_X, CAL_BTN_Y, CAL_BTN_W, CAL_BTN_H) != 0u) {
                hold_ms = 0u;
                while (TOUCH_IsPressed() != 0u) {
                    Delay_Ms(CAL_WAIT_MS);
                    hold_ms = (uint16_t)(hold_ms + CAL_WAIT_MS);
                    if (hold_ms >= CAL_RUN_HOLD_MS) {
                        WaitTouchRelease();
                        RunCalibration();
                        UI_DrawMain();
                        UI_ShowState(pb0_on, pb1_on);
                        break;
                    }
                }
                WaitTouchRelease();
            } else if (TouchRawInButtonBand(raw_x, raw_y, &is_left) != 0u) {
                if (is_left != 0u) {
                    pb0_on ^= 1u;
                } else {
                    pb1_on ^= 1u;
                }

                LED_Set(pb0_on, pb1_on);
                UI_ShowState(pb0_on, pb1_on);
                WaitTouchRelease();
            } else {
                WaitTouchRelease();
            }
        }

        Delay_Ms(6u);
    }
}