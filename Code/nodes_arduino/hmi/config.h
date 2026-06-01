#pragma once
// ── GPIO Waveshare ESP32-S3-Knob-Touch-LCD-1.8 ───────────────────────────────
// Display QSPI ST77916
#define PIN_LCD_CS    10
#define PIN_LCD_CLK   12
#define PIN_LCD_D0    11
#define PIN_LCD_D1    13
#define PIN_LCD_D2    7
#define PIN_LCD_D3    14
#define PIN_LCD_RST   9
#define PIN_LCD_DC    8
#define PIN_LCD_BL    47   // backlight PWM

// Touch CST816 I2C
#define PIN_TOUCH_SDA 15
#define PIN_TOUCH_SCL 16
#define PIN_TOUCH_INT 6
#define TOUCH_I2C_ADDR 0x15

// Encoder (via ESP32-U4WDH su UART interno)
#define PIN_ENC_UART_RX 18
#define PIN_ENC_UART_TX 17

// Audio PCM5100A I2S
#define PIN_I2S_BCLK  40
#define PIN_I2S_WS    39
#define PIN_I2S_DOUT  38

// Vibrazione DRV2605 I2C (stesso bus del touch)
#define DRV2605_I2C_ADDR 0x5A

#define DISPLAY_WIDTH   360
#define DISPLAY_HEIGHT  360
