#include "lcd_st7789v_16bit.h"

#include "delay.h"
#include "font5x7.h"

/* Pin mapping from your xlsx sheet (F103VE) */
#define LCD_PIN_RES GPIO_Pin_1   /* PE1  */
#define LCD_PIN_D15 GPIO_Pin_10  /* PD10 */
#define LCD_PIN_D14 GPIO_Pin_9   /* PD9  */
#define LCD_PIN_D13 GPIO_Pin_8   /* PD8  */
#define LCD_PIN_D12 GPIO_Pin_15  /* PE15 */
#define LCD_PIN_D11 GPIO_Pin_14  /* PE14 */
#define LCD_PIN_D10 GPIO_Pin_13  /* PE13 */
#define LCD_PIN_D9  GPIO_Pin_12  /* PE12 */
#define LCD_PIN_D8  GPIO_Pin_11  /* PE11 */
#define LCD_PIN_D7  GPIO_Pin_10  /* PE10 */
#define LCD_PIN_D6  GPIO_Pin_9   /* PE9  */
#define LCD_PIN_D5  GPIO_Pin_8   /* PE8  */
#define LCD_PIN_D4  GPIO_Pin_7   /* PE7  */
#define LCD_PIN_D3  GPIO_Pin_1   /* PD1  */
#define LCD_PIN_D2  GPIO_Pin_0   /* PD0  */
#define LCD_PIN_D1  GPIO_Pin_15  /* PD15 */
#define LCD_PIN_D0  GPIO_Pin_14  /* PD14 */
#define LCD_PIN_RD  GPIO_Pin_4   /* PD4  */
#define LCD_PIN_WR  GPIO_Pin_5   /* PD5  */
#define LCD_PIN_RS  GPIO_Pin_11  /* PD11 */
#define LCD_PIN_CS  GPIO_Pin_7   /* PD7  */
#define LCD_PIN_BK  GPIO_Pin_12  /* PD12, low level = backlight ON */

#define LCD_PORT_D GPIOD
#define LCD_PORT_E GPIOE

#define LCD_D_MASK_D (LCD_PIN_D15 | LCD_PIN_D14 | LCD_PIN_D13 | LCD_PIN_D3 | LCD_PIN_D2 | LCD_PIN_D1 | LCD_PIN_D0)
#define LCD_D_MASK_E (LCD_PIN_D12 | LCD_PIN_D11 | LCD_PIN_D10 | LCD_PIN_D9 | LCD_PIN_D8 | LCD_PIN_D7 | LCD_PIN_D6 | LCD_PIN_D5 | LCD_PIN_D4)

static inline void LCD_CS_L(void)  { GPIO_ResetBits(LCD_PORT_D, LCD_PIN_CS); }
static inline void LCD_CS_H(void)  { GPIO_SetBits(LCD_PORT_D, LCD_PIN_CS); }
static inline void LCD_RS_L(void)  { GPIO_ResetBits(LCD_PORT_D, LCD_PIN_RS); }
static inline void LCD_RS_H(void)  { GPIO_SetBits(LCD_PORT_D, LCD_PIN_RS); }
static inline void LCD_WR_L(void)  { GPIO_ResetBits(LCD_PORT_D, LCD_PIN_WR); }
static inline void LCD_WR_H(void)  { GPIO_SetBits(LCD_PORT_D, LCD_PIN_WR); }
static inline void LCD_RD_H(void)  { GPIO_SetBits(LCD_PORT_D, LCD_PIN_RD); }
static inline void LCD_RST_L(void) { GPIO_ResetBits(LCD_PORT_E, LCD_PIN_RES); }
static inline void LCD_RST_H(void) { GPIO_SetBits(LCD_PORT_E, LCD_PIN_RES); }
static inline void LCD_BK_ON(void) { GPIO_ResetBits(LCD_PORT_D, LCD_PIN_BK); }

static inline void LCD_WR_STROBE(void)
{
    LCD_WR_L();
    __NOP();
    LCD_WR_H();
}

static void LCD_WriteBus16(uint16_t value)
{
    uint16_t d = 0u;
    uint16_t e = 0u;

    if (value & (1u << 15)) d |= LCD_PIN_D15;
    if (value & (1u << 14)) d |= LCD_PIN_D14;
    if (value & (1u << 13)) d |= LCD_PIN_D13;
    if (value & (1u << 12)) e |= LCD_PIN_D12;
    if (value & (1u << 11)) e |= LCD_PIN_D11;
    if (value & (1u << 10)) e |= LCD_PIN_D10;
    if (value & (1u << 9))  e |= LCD_PIN_D9;
    if (value & (1u << 8))  e |= LCD_PIN_D8;
    if (value & (1u << 7))  e |= LCD_PIN_D7;
    if (value & (1u << 6))  e |= LCD_PIN_D6;
    if (value & (1u << 5))  e |= LCD_PIN_D5;
    if (value & (1u << 4))  e |= LCD_PIN_D4;
    if (value & (1u << 3))  d |= LCD_PIN_D3;
    if (value & (1u << 2))  d |= LCD_PIN_D2;
    if (value & (1u << 1))  d |= LCD_PIN_D1;
    if (value & (1u << 0))  d |= LCD_PIN_D0;

    LCD_PORT_D->BSRR = ((uint32_t)LCD_D_MASK_D << 16) | d;
    LCD_PORT_E->BSRR = ((uint32_t)LCD_D_MASK_E << 16) | e;
}

static void LCD_WriteCmd(uint8_t cmd)
{
    LCD_RS_L();
    LCD_WriteBus16((uint16_t)cmd);
    LCD_WR_STROBE();
}

static void LCD_WriteData8(uint8_t data)
{
    LCD_RS_H();
    LCD_WriteBus16((uint16_t)data);
    LCD_WR_STROBE();
}

static void LCD_WriteData16(uint16_t data)
{
    LCD_RS_H();
    LCD_WriteBus16(data);
    LCD_WR_STROBE();
}

static void LCD_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    LCD_WriteCmd(0x2A);
    LCD_WriteData8((uint8_t)(x0 >> 8));
    LCD_WriteData8((uint8_t)(x0 & 0xFF));
    LCD_WriteData8((uint8_t)(x1 >> 8));
    LCD_WriteData8((uint8_t)(x1 & 0xFF));

    LCD_WriteCmd(0x2B);
    LCD_WriteData8((uint8_t)(y0 >> 8));
    LCD_WriteData8((uint8_t)(y0 & 0xFF));
    LCD_WriteData8((uint8_t)(y1 >> 8));
    LCD_WriteData8((uint8_t)(y1 & 0xFF));

    LCD_WriteCmd(0x2C);
}

static void LCD_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE, ENABLE);

    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;

    gpio.GPIO_Pin = LCD_D_MASK_D | LCD_PIN_RD | LCD_PIN_WR | LCD_PIN_RS | LCD_PIN_CS | LCD_PIN_BK;
    GPIO_Init(LCD_PORT_D, &gpio);

    gpio.GPIO_Pin = LCD_D_MASK_E | LCD_PIN_RES;
    GPIO_Init(LCD_PORT_E, &gpio);

    LCD_RD_H();
    LCD_WR_H();
    LCD_CS_H();
    LCD_RS_H();
    LCD_RST_H();
}

void LCD_Init(void)
{
    LCD_GPIO_Init();

    LCD_CS_L();

    LCD_RST_L();
    Delay_Ms(20);
    LCD_RST_H();
    Delay_Ms(120);

    LCD_WriteCmd(0x11);
    Delay_Ms(120);

    LCD_WriteCmd(0x36);
    LCD_WriteData8(0x00);

    LCD_WriteCmd(0x3A);
    LCD_WriteData8(0x55);

    LCD_WriteCmd(0xB2);
    LCD_WriteData8(0x0C); LCD_WriteData8(0x0C); LCD_WriteData8(0x00); LCD_WriteData8(0x33); LCD_WriteData8(0x33);

    LCD_WriteCmd(0xB7);
    LCD_WriteData8(0x35);

    LCD_WriteCmd(0xBB);
    LCD_WriteData8(0x1F);

    LCD_WriteCmd(0xC0);
    LCD_WriteData8(0x2C);

    LCD_WriteCmd(0xC2);
    LCD_WriteData8(0x01);

    LCD_WriteCmd(0xC3);
    LCD_WriteData8(0x12);

    LCD_WriteCmd(0xC4);
    LCD_WriteData8(0x20);

    LCD_WriteCmd(0xC6);
    LCD_WriteData8(0x0F);

    LCD_WriteCmd(0xD0);
    LCD_WriteData8(0xA4); LCD_WriteData8(0xA1);

    LCD_WriteCmd(0xE0);
    LCD_WriteData8(0xD0); LCD_WriteData8(0x08); LCD_WriteData8(0x11); LCD_WriteData8(0x08);
    LCD_WriteData8(0x0C); LCD_WriteData8(0x15); LCD_WriteData8(0x39); LCD_WriteData8(0x33);
    LCD_WriteData8(0x50); LCD_WriteData8(0x36); LCD_WriteData8(0x13); LCD_WriteData8(0x14);
    LCD_WriteData8(0x29); LCD_WriteData8(0x2D);

    LCD_WriteCmd(0xE1);
    LCD_WriteData8(0xD0); LCD_WriteData8(0x08); LCD_WriteData8(0x10); LCD_WriteData8(0x08);
    LCD_WriteData8(0x06); LCD_WriteData8(0x06); LCD_WriteData8(0x39); LCD_WriteData8(0x44);
    LCD_WriteData8(0x51); LCD_WriteData8(0x0B); LCD_WriteData8(0x16); LCD_WriteData8(0x14);
    LCD_WriteData8(0x2F); LCD_WriteData8(0x31);

    LCD_WriteCmd(0x21);
    LCD_WriteCmd(0x29);

    LCD_BK_ON();
}

void LCD_Clear(uint16_t color)
{
    LCD_FillRect(0u, 0u, LCD_WIDTH, LCD_HEIGHT, color);
}

void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        return;
    }

    LCD_SetAddressWindow(x, y, x, y);
    LCD_WriteData16(color);
}

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint16_t x1;
    uint16_t y1;
    uint32_t total;
    uint32_t i;

    if (x >= LCD_WIDTH || y >= LCD_HEIGHT || w == 0u || h == 0u) {
        return;
    }

    x1 = (uint16_t)(x + w - 1u);
    y1 = (uint16_t)(y + h - 1u);

    if (x1 >= LCD_WIDTH) {
        x1 = LCD_WIDTH - 1u;
    }
    if (y1 >= LCD_HEIGHT) {
        y1 = LCD_HEIGHT - 1u;
    }

    LCD_SetAddressWindow(x, y, x1, y1);

    total = (uint32_t)(x1 - x + 1u) * (uint32_t)(y1 - y + 1u);
    for (i = 0u; i < total; i++) {
        LCD_WriteData16(color);
    }
}

void LCD_DrawChar5x7(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale)
{
    const uint8_t *glyph;
    uint8_t row;
    uint8_t col;
    uint8_t sx;
    uint8_t sy;

    if (scale == 0u) {
        return;
    }

    glyph = Font5x7_Get(c);

    for (row = 0u; row < 7u; row++) {
        for (col = 0u; col < 5u; col++) {
            uint16_t color = (glyph[row] & (1u << (4u - col))) ? fg : bg;
            for (sy = 0u; sy < scale; sy++) {
                for (sx = 0u; sx < scale; sx++) {
                    LCD_DrawPixel((uint16_t)(x + col * scale + sx), (uint16_t)(y + row * scale + sy), color);
                }
            }
        }
    }
}

void LCD_DrawString(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (s == 0) {
        return;
    }

    while (*s != '\0') {
        LCD_DrawChar5x7(x, y, *s, fg, bg, scale);
        x = (uint16_t)(x + 6u * scale);
        s++;
    }
}
