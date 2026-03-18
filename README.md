# stm32-learn

This repository contains a Keil/uVision project for `STM32F103VE`.
It drives a `ST7789V` TFT LCD and an `XPT2046` resistive touch
controller, then uses a simple touch UI to toggle the board LEDs on
`PB0` and `PB1`.

The project is intended as a small STM32 learning example for display,
touch input, and calibration flow.

## Features

- 240x320 portrait LCD UI
- Two on-screen touch buttons
- Touch left button to toggle `PB0`
- Touch right button to toggle `PB1`
- Touch calibration during boot
- Runtime recalibration by holding the `CAL` button

## Hardware And Toolchain

- MCU: `STM32F103VE`
- LCD controller: `ST7789V`
- Touch controller: `XPT2046`
- IDE: `Keil uVision5`
- Compiler: `ARMCLANG`
- Project file: `eg.uvprojx`

## Project Layout

```text
.
|-- BSP/        Board support drivers for LCD, touch, delay, and font
|-- Library/    STM32F10x Standard Peripheral Library
|-- RTE/        Device debug configuration
|-- start/      Startup files and system code
|-- user/       Application entry and interrupt handlers
|-- eg.sct      Scatter-loading file
`-- eg.uvprojx  Keil project
```

## Main Files

- `user/main.c`
  Main UI flow, LED control logic, touch handling, and calibration
- `BSP/lcd_st7789v_16bit.c`
  LCD low-level driver and drawing helpers
- `BSP/touch_xpt2046.c`
  Touch sampling and calibration helpers
- `BSP/delay.c`
  Delay utilities

## How To Build

1. Open `eg.uvprojx` in Keil uVision.
2. Select `Target_1`.
3. Build the project.
4. Download the image to the board.

## How To Use

1. Power on the board.
2. If calibration is needed, keep touching the screen within 3 seconds
   after boot.
3. After the main UI appears:
   - Touch the left button to toggle `PB0`
   - Touch the right button to toggle `PB1`
   - Hold the `CAL` button in the top-right corner to recalibrate

## Calibration Flow

The firmware supports two ways to enter calibration:

- Hold the touch panel during the boot window
- Hold the on-screen `CAL` button while the program is running

During calibration, the firmware asks for four points in order:

1. Left top
2. Right top
3. Left bottom
4. Right bottom

If calibration succeeds, the screen shows `CAL OK`.
If it fails, the screen shows `CAL FAIL`.

## Notes

- The repository keeps source files, project files, and required config
- Build outputs and local Keil user files such as `*.uvoptx` and
  `*.uvguix.*` are ignored by `.gitignore`
