#pragma once
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <i2cdev.h>


#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include "esp_lcd_panel_vendor.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

#include "board.h"
#include "utill.h"
#include "rtc_ds3231.h"
#include "dht_sensor.h"
#include "wifi_conn.h"

#include "state_manager.h"

// #include "driver/i2c.h"

#define I2C_HOST  0

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define DISPLAY_PIN_NUM_RST           -1
#define DISPLAY_I2C_HW_ADDR           0x3C

// The pixel number in horizontal and vertical
#define DISPLAY_LCD_H_RES              128
#define DISPLAY_LCD_V_RES              64
// Bit number used to represent command and parameter
#define DISPLAY_LCD_CMD_BITS           8
#define DISPLAY_LCD_PARAM_BITS         8


extern void init_lvgl_lcd_display(void);
