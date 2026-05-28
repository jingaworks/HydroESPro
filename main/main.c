// main.c
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "pins.h"
#include "event_bus.h"
#include "storage_manager.h"
#include "i2c_manager.h"
#include "rtc_manager.h"
#include "time_manager.h"
#include "pcf_manager.h"
#include "lcd_manager.h"
#include "wifi_manager.h"
// #include "state_manager.h"
// #include "hydro_manager.h"

static const char* TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, "       HydroESPro v1.0 Starting...       ");
    ESP_LOGI(TAG, "=========================================");

    // ==================== INIȚIALIZARE CRITICĂ ====================
    
    // 1. Event Bus
    ESP_ERROR_CHECK(event_bus_init());

    // 2. Storage Manager (foarte important - sus)
    ESP_ERROR_CHECK(storage_manager_init());

    // 3. I2C
    ESP_ERROR_CHECK(i2c_manager_init(I2C_SDA_PIN, I2C_SCL_PIN, 400000));

    // 4. I2C Scan (debug)
    i2c_manager_scan();

    // 5. RTC DS3231 (timp real)
    ESP_ERROR_CHECK(rtc_manager_init());
    rtc_manager_set_system_time();     // Sincronizează ESP32 time cu RTC

    // 6. Time Manager
    ESP_ERROR_CHECK(time_manager_init());

    // 7. PCF8574
    ESP_ERROR_CHECK(pcf_manager_init());

    // 8. LCD
    ESP_ERROR_CHECK(lcd_manager_init());

    // 9. WiFi (după ce avem timp corect)
    ESP_ERROR_CHECK(wifi_manager_init());

    // 10. Restul modulelor
    // ESP_ERROR_CHECK(state_manager_init());
    // ESP_ERROR_CHECK(hydro_manager_init());

    ESP_LOGI(TAG, "=== HydroESPro fully initialized! ===");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

