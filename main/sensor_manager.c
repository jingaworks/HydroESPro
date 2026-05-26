#include "sensor_manager.h"
#include "time_manager.h"
#include "state_manager.h"
#include <esp_log.h>

static const char *TAG = "SENSOR_MANAGER";

static bool sensors_ready = false;

// =============================================
// COMMON LOGGING HELPER
// =============================================
esp_err_t sensor_log_reading(sensor_type_t type, float temp, float humidity, bool valid)
{
    struct tm tm = time_manager_get_tm();
    char log_line[128];

    if (!valid) {
        snprintf(log_line, sizeof(log_line), "\n%02u:%02u:%02u - INVALID READING", 
                 tm.tm_hour, tm.tm_min, tm.tm_sec);
    } else if (type == SENSOR_DS18B20) {
        snprintf(log_line, sizeof(log_line), "\n%02u:%02u:%02u - %.1fC", 
                 tm.tm_hour, tm.tm_min, tm.tm_sec, temp);
    } else {
        snprintf(log_line, sizeof(log_line), "\n%02u:%02u:%02u - %.1fC %.1fRH", 
                 tm.tm_hour, tm.tm_min, tm.tm_sec, temp, humidity);
    }

    log_type_t log_type = (type == SENSOR_DHT) ? LOG_TYPE_DHT : LOG_TYPE_DS18B20;

    return storage_log_write(log_type, log_line, strlen(log_line));
}

// =============================================
// EVENT HANDLER
// =============================================
static void sensor_event_handler(void *arg, esp_event_base_t base, 
                                int32_t event_id, void *event_data)
{
    if (base != SYSTEM_EVENTS) return;

    // Poți adăuga reacții la stări sistem (ex: oprire senzori în mentenanță)
    if (event_id == SYS_EVENT_STATE_CHANGED) {
        state_change_event_t *ev = (state_change_event_t *)event_data;
        if (ev->new_state == SYS_STATE_MAINTENANCE) {
            ESP_LOGI(TAG, "Maintenance mode → sensors paused for logging");
        }
    }
}

// =============================================
// PUBLIC API
// =============================================
esp_err_t sensor_manager_init(void)
{
    ESP_LOGI(TAG, "Sensor Manager initialized");

    // Abonare la evenimente de sistem
    event_bus_register_handler(SYSTEM_EVENTS, SYS_EVENT_STATE_CHANGED, 
                              sensor_event_handler, NULL);

    sensors_ready = true;
    return ESP_OK;
}

esp_err_t sensor_register_dht(void)
{
    ESP_LOGI(TAG, "DHT Sensor registered");
    return ESP_OK;
}

esp_err_t sensor_register_ds18b20(void)
{
    ESP_LOGI(TAG, "DS18B20 Sensor registered");
    return ESP_OK;
}

bool sensor_manager_is_ready(void)
{
    return sensors_ready;
}


// #include <stdio.h>
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>
// #include <i2cdev.h>
// #include <aht.h>
// #include <bmp280.h>

// #define SDA_GPIO GPIO_NUM_21
// #define SCL_GPIO GPIO_NUM_22

// void sensor_task(void *pvParameter)
// {
//     // Initialize AHT20
//     aht_t aht_dev = { 0 };
//     aht_dev.i2c_dev.port = 0;
//     aht_dev.i2c_dev.addr = AHT_I2C_ADDRESS_GND; // 0x38
//     aht_init(&aht_dev, AHT_TYPE_AHT20, 0, SDA_GPIO, SCL_GPIO);

//     // Initialize BMP280
//     bmp280_params_t bmp_params;
//     bmp280_init_default_params(&bmp_params);
//     bmp280_t bmp_dev = { 0 };
//     bmp280_init_desc(&bmp_dev, BMP280_I2C_ADDRESS_0, 0, SDA_GPIO, SCL_GPIO);
//     bmp280_init(&bmp_dev, &bmp_params);

//     float temp_aht, humidity_aht;
//     float temp_bmp, pressure_bmp, humidity_ignored;

//     while (1) {
//         // Read AHT20
//         if (aht_get_data(&aht_dev, &temp_aht, &humidity_aht) == ESP_OK) {
//             printf("AHT20 -> Temp: %.2f C, Humidity: %.2f%%\n", temp_aht, humidity_aht);
//         }

//         // Read BMP280
//         if (bmp280_read_float(&bmp_dev, &temp_bmp, &pressure_bmp, &humidity_ignored) == ESP_OK) {
//             printf("BMP280 -> Temp: %.2f C, Pressure: %.2f Pa\n", temp_bmp, pressure_bmp);
//         }

//         vTaskDelay(pdMS_TO_TICKS(2000));
//     }
// }

// void app_main(void)
// {
//     i2cdev_init();
//     xTaskCreate(&sensor_task, "sensor_task", 4096, NULL, 5, NULL);
// }
