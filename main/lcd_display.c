#include "lcd_display.h"
#include "state_manager.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "LCD_DISPLAY";

esp_lcd_panel_handle_t panel_handle = NULL;

// Obiecte LVGL globale - Pointeri direcți securizați (Fără indexare)
lv_obj_t *screen_container;
lv_timer_t *screen_timer;
lv_obj_t *label_title;
lv_obj_t *label_value;

static int8_t display_id = 0;
char display_title_1[32] = { 0 };
char display_value[32] = { 0 };

static void change_screen_id(int8_t *current_id) {
    if (!dht_valid_reading && !ds_valid_reding) {
        *current_id = 3;
        return;
    }

    if (*current_id < 2) {
        *current_id += 1;
    }
    else {
        *current_id = 0;
    }

    if (*current_id == 0 && !dht_valid_reading) {
        *current_id = 2;
    }
    if (*current_id == 1 && !dht_valid_reading) {
        *current_id = 2;
    }
    if (*current_id == 2 && !ds_valid_reding) {
        *current_id = 0;
    }
}
// =============================================
// EVENT HANDLER
// =============================================
static void system_state_event_handler(void *arg, esp_event_base_t base, 
                                      int32_t event_id, void *event_data)
{
    if (base != SYSTEM_EVENTS) return;

    switch (event_id) {
        case SYS_EVENT_STATE_CHANGED: {
            state_change_event_t *ev = (state_change_event_t *)event_data;
            ESP_LOGI(TAG, "LCD: State changed %d -> %d", ev->old_state, ev->new_state);
            
            // Forțăm refresh imediat la schimbare de stare
            if (ev->new_state == SYS_STATE_ALARM || 
                ev->new_state == SYS_STATE_MAINTENANCE) {
                display_id = 0; // reset la prima ecran
            }
            break;
        }

        case SYS_EVENT_ALARM_TRIGGERED:
            ESP_LOGW(TAG, "LCD: Alarm triggered!");
            display_id = 0; // forțează afișare alarmă
            break;

        case SYS_EVENT_MAINTENANCE_START:
            ESP_LOGI(TAG, "LCD: Maintenance mode started");
            break;
    }
}

// =============================================
// DISPLAY TIMER (logic original + state awareness)
// =============================================
static void display_timer(lv_timer_t *timer)
{
    system_state_t current_state = state_manager_get_state();

    lv_obj_set_style_text_font(label_value, &lv_font_montserrat_28, LV_PART_MAIN);

    if (current_state == SYS_STATE_ALARM) {
        strcpy(display_title_1, "! ATENTIE !");
        lv_obj_set_style_text_font(label_value, &lv_font_montserrat_20, LV_PART_MAIN);

        if (sys_manager.tank_low_flag) {
            strcpy(display_value, "BAZIN GOL");
        } else if (sys_manager.temp_error_flag) {
            strcpy(display_value, "APA CALDA");
        } else {
            strcpy(display_value, "ALARMA");
        }
    }
    else if (current_state == SYS_STATE_MAINTENANCE) {
        strcpy(display_title_1, "SERVICE");
        lv_obj_set_style_text_font(label_value, &lv_font_montserrat_20, LV_PART_MAIN);
        strcpy(display_value, "SCHIMB APA");
    }
    else if (current_state == SYS_STATE_INIT) {
        strcpy(display_title_1, "SISTEM");
        lv_obj_set_style_text_font(label_value, &lv_font_montserrat_20, LV_PART_MAIN);
        strcpy(display_value, "PORNIRE");
    }
    else {
        // Normal rotation
        if (display_id == 0) {
            strcpy(display_title_1, "TEMP AER");
            snprintf(display_value, sizeof(display_value), "%.1f C", dht_temp);
        }
        else if (display_id == 1) {
            strcpy(display_title_1, "UMIDITATE");
            snprintf(display_value, sizeof(display_value), "%.0f %%", dht_humid);
        }
        else if (display_id == 2) {
            strcpy(display_title_1, "TEMP APA");
            snprintf(display_value, sizeof(display_value), "%.1f C", ds_temp);
        }

        change_screen_id(&display_id);
    }

    lv_label_set_text(label_title, display_title_1);
    lv_label_set_text(label_value, display_value);
}

static void ui_main_screen(void) {
    lv_obj_t *screen = lv_disp_get_scr_act(NULL);
    lv_obj_set_style_bg_opa(screen, LV_OPA_TRANSP, LV_PART_MAIN);

    screen_container = lv_obj_create(screen);
    lv_obj_set_size(screen_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(screen_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen_container, 0, LV_PART_MAIN);

    lv_obj_set_style_pad_all(screen_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(screen_container, 4, LV_PART_MAIN); // 4px gap e perfect pentru textul de 28px

    lv_obj_align(screen_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_layout(screen_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_flex_main_place(screen_container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(screen_container, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);

    // 1. Eticheta de sus: Titlu mic (Montserrat 12)
    label_title = lv_label_create(screen_container);
    lv_label_set_text(label_title, "SISTEM");
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // 2. Eticheta centrală: VALOARE URIAȘĂ DE 28PX (Ușor de văzut pentru ochi)
    label_value = lv_label_create(screen_container);
    lv_label_set_text(label_value, "START");
    lv_obj_set_style_text_font(label_value, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

void init_lvgl_lcd_display(void) {
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = DISPLAY_I2C_HW_ADDR,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = DISPLAY_LCD_CMD_BITS,
        .lcd_param_bits = DISPLAY_LCD_CMD_BITS,
        .dc_bit_offset = 6,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install SSD1306 panel driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = DISPLAY_PIN_NUM_RST,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, false));

    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = DISPLAY_LCD_H_RES * DISPLAY_LCD_V_RES,
        .double_buffer = true,
        .hres = DISPLAY_LCD_H_RES,
        .vres = DISPLAY_LCD_V_RES,
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        }
    };
    lv_disp_t *lcd_disp = lvgl_port_add_disp(&disp_cfg);

    lv_disp_set_rotation(lcd_disp, LV_DISP_ROT_180);

    /* Inițializare interfață pe 2 linii */
    ui_main_screen();
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    /* Înregistrare timer la 3 secunde */
    screen_timer = lv_timer_create(display_timer, 3000, NULL);

    // ==================== ABONARE LA EVENIMENTE ====================
    event_bus_register_handler(SYSTEM_EVENTS, SYS_EVENT_STATE_CHANGED,
        system_state_event_handler, NULL);

    event_bus_register_handler(SYSTEM_EVENTS, SYS_EVENT_ALARM_TRIGGERED,
        system_state_event_handler, NULL);

    event_bus_register_handler(SYSTEM_EVENTS, SYS_EVENT_MAINTENANCE_START,
        system_state_event_handler, NULL);

    ESP_LOGI(TAG, "LCD Display initialized with event bus");
}
