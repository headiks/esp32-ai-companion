// Pin map of ESP32-S3-Touch-AMOLED-1.75C. Source: schematic ESP32-S3-Touch-AMOLED-1.75C-schematic.pdf and
// examples/arduino/libraries/board_config/pin_config.h of the official repository, see documentation/HARDWARE.md.
// The non-C board differs (LCD reset 39, touch reset 40, MCLK 42): do not mix them up.
#pragma once

// CO5300 AMOLED over QSPI
#define PIN_LCD_CS 12
#define PIN_LCD_SCLK 38
#define PIN_LCD_D0 4
#define PIN_LCD_D1 5
#define PIN_LCD_D2 6
#define PIN_LCD_D3 7
#define PIN_LCD_RESET 1
#define PIN_LCD_TE 13
#define LCD_COL_OFFSET 6  // from the official example

// Shared I2C: CST9217 0x5A, QMI8658 0x6B, AXP2101 0x34, ES8311 0x18, ES7210 0x40
#define PIN_I2C_SDA 15
#define PIN_I2C_SCL 14
#define PIN_TP_INT 11
#define PIN_TP_RESET 2
#define PIN_IMU_INT1 21

// I2S: ES8311 playback and ES7210 capture share all clocks
#define PIN_I2S_MCLK 16
#define PIN_I2S_BCLK 9
#define PIN_I2S_LRCK 45
#define PIN_I2S_DOUT 8
#define PIN_I2S_DIN 10
#define PIN_PA_CTRL 46  // NS4150B CTRL, R31 pulls it low: active high

// Buttons
#define PIN_BOOT 0
#define PIN_SYS_OUT 3   // PWR key state, high while pressed
