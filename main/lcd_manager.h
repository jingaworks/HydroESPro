#pragma once

#include "esp_err.h"
#include "lvgl.h"

esp_err_t lcd_manager_init(void);

void lcd_set_backlight(uint8_t percent);        // 0 - 100%
void lcd_turn_on(void);
void lcd_turn_off(void);

lv_display_t* lcd_manager_get_display(void);

// Funcții UI de bază (vom extinde ulterior)
void lcd_show_startup_screen(void);
void lcd_show_main_screen(void);

void lcd_manager_lock(void);
void lcd_manager_unlock(void);