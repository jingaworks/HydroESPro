#include "lcd_manager.h"
#include "pins.h"
#include "event_bus.h"

#include <sys/lock.h>
#include <sys/param.h>
#include <stdbool.h>
#include <esp_log.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_timer.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <lvgl.h>

#include "esp_lcd_ili9341.h"

static const char *TAG = "LCD_MGR";

static lv_display_t *display = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static SemaphoreHandle_t lvgl_mutex = NULL;

// Using SPI2
#define LCD_HOST                SPI2_HOST
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ   (20 * 1000 * 1000)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  1
#define EXAMPLE_LCD_H_RES       240
#define EXAMPLE_LCD_V_RES       320
#define EXAMPLE_LVGL_DRAW_BUF_LINES  20
#define EXAMPLE_LVGL_TICK_PERIOD_MS  2

static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
static void example_lvgl_tick(void *arg);
static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);

// ====================== INIT ======================
esp_err_t lcd_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing LCD Manager (ILI9341 2.8\")");

    lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (lvgl_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL mutex");
        return ESP_FAIL;
    }

    // Backlight GPIO
    gpio_set_direction(LCD_BL_PIN, GPIO_MODE_OUTPUT);
    lcd_set_backlight(80);                     // 80% inițial

    // SPI Bus
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_SCK_PIN,
        .mosi_io_num = LCD_MOSI_PIN,
        .miso_io_num = LCD_MISO_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // Panel IO
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_DC_PIN,
        .cs_gpio_num = LCD_CS_PIN,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    // ILI9341 Panel
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // LVGL Init
    lv_init();

    display = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);

    size_t draw_buffer_sz = EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_DRAW_BUF_LINES * sizeof(lv_color16_t);
    void *buf1 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
    void *buf2 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
    
    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(display, panel_handle);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, example_lvgl_flush_cb);

    // LVGL Tick Timer
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    // Flush ready callback
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = example_notify_lvgl_flush_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display));

    ESP_LOGI(TAG, "LCD + LVGL initialized successfully");

    lcd_show_startup_screen();

    return ESP_OK;
}

// ====================== BACKLIGHT ======================
void lcd_set_backlight(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t duty = (percent * 255) / 100;     // 8-bit
    gpio_set_level(LCD_BL_PIN, duty > 0 ? 1 : 0);   // Simplu ON/OFF pentru moment
    // Poți adăuga PWM mai târziu cu LEDC
}

void lcd_turn_on(void)  { lcd_set_backlight(80); }
void lcd_turn_off(void) { lcd_set_backlight(0); }

// ====================== LVGL HELPERS ======================
void lcd_manager_lock(void)
{
    xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
}

void lcd_manager_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mutex);
}

lv_display_t* lcd_manager_get_display(void)
{
    return display;
}

// ====================== CALLBACKS ======================
static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    lv_draw_sw_rgb565_swap(px_map, (offsetx2 + 1 - offsetx1) * (offsety2 + 1 - offsety1));

    esp_lcd_panel_draw_bitmap(panel, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
    lv_display_flush_ready(disp);
}

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void example_lvgl_tick(void *arg)
{
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

// ====================== SCREENS ======================
void lcd_show_startup_screen(void)
{
    lcd_manager_lock();
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "HydroESPro\nv1.0\nInitializing...");
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    lcd_manager_unlock();
}

void lcd_show_main_screen(void)
{
    // Vom implementa mai târziu
    lcd_manager_lock();
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Main Screen\nHydroESPro");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    lcd_manager_unlock();
}