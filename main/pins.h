// =============================================
//            HydroESPro - PINOUT
// =============================================

#pragma once

// ====================== I2C ======================
#define I2C_SDA_PIN             21
#define I2C_SCL_PIN             22

// ====================== LCD 2.8" ILI9341 (VSPI) ======================
#define LCD_CS_PIN              5
#define LCD_DC_PIN              4
#define LCD_RST_PIN             -1          // Software reset
#define LCD_BL_PIN              25          // Backlight PWM
#define LCD_SCK_PIN             18
#define LCD_MOSI_PIN            23
#define LCD_MISO_PIN            19

// ====================== Senzori ======================
#define DS18B20_PIN             27

// ====================== PCF8574 ======================
#define PCF_ENABLE_PIN          33          // Activează modulele PCF
#define PCF_INTERRUPT_PIN       32          // Interrupt de la PCF Input

// ====================== SDMMC ======================
#define SDMMC_CLK_PIN           14          // CLK
#define SDMMC_CMD_PIN           15          // MOSI
#define SDMMC_D0_PIN            2           // MISO

// ====================== Pini Liberi ======================
#define GPIO_FREE_12            12
#define GPIO_FREE_13            13
#define GPIO_FREE_16            16
#define GPIO_FREE_17            17
#define GPIO_FREE_26            26
#define GPIO_FREE_34            34   // Input only
#define GPIO_FREE_35            35   // Input only
#define GPIO_FREE_36            36   // Input only
#define GPIO_FREE_39            39   // Input only

// =============================================
//               I2C DEVICE ADDRESSES
// =============================================

#define PCF8574_OUTPUT_ADDR     0x20    // Relee, LED-uri, Buzzer
#define PCF8574_INPUT_ADDR      0x21    // Butoane + Senzor nivel
#define AHT20_ADDR              0x38
#define BMP280_ADDR             0x76
#define DS3231_ADDR             0x68


// =============================================
//               PCF8574 - OUTPUTS
// =============================================
#define RELAY_WATER_PUMP        0   // P0
#define RELAY_AIR_PUMP          1   // P1
#define RELAY_DOSER_PH_DOWN     2   // P2
#define RELAY_DOSER_PH_UP       3   // P3
#define RELAY_DOSER_NUTRI       4   // P4
#define LED_STATUS              5   // P5
#define BUZZER_PIN              6   // P6
#define SPARE_OUTPUT_7          7   // P7

// =============================================
//               PCF8574 - INPUTS
// =============================================
#define BUTTON_MENU             0   // P0
#define BUTTON_UP               1   // P1
#define BUTTON_DOWN             2   // P2
#define BUTTON_ENTER            3   // P3
#define BUTTON_BACK             4   // P4
#define FLOAT_SWITCH_LEVEL      5   // P5 - Senzor nivel bazin
#define SPARE_INPUT_6           6
#define SPARE_INPUT_7           7

