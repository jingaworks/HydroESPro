#include "dht_sensor.h"
#include "sensor_manager.h"
#include "time_manager.h"
#include "storage_manager.h"
#include "event_bus.h"
#include <esp_log.h>

static const char *TAG = "DHT";

TaskHandle_t DHT_TaskHandle = NULL;

float dht_temp = 0.0f;
float dht_humid = 0.0f;
bool dht_valid_reading = false;

static uint8_t dht_error_count = 0;

// Citire cu retry
static esp_err_t read_dht_with_retry(float *humidity, float *temperature)
{
    esp_err_t ret;
    
    for (int attempt = 0; attempt < 3; attempt++) {
        ret = dht_read_float_data(DHT_SENSOR_TYPE, SENSOR_DHT_GPIO, humidity, temperature);
        
        if (ret == ESP_OK) {
            return ESP_OK;
        }
        
        vTaskDelay(pdMS_TO_TICKS(300 + attempt * 200)); // backoff
    }
    
    return ret;
}

static void read_dht_task(void *pvParameter)
{
    TickType_t xLastWakeTime;
    int8_t count_log_time = 0;

    for (;;) {
        xLastWakeTime = xTaskGetTickCount();
        
        dht_valid_reading = false;
        
        if (read_dht_with_retry(&dht_humid, &dht_temp) == ESP_OK) {
            dht_valid_reading = true;
            dht_error_count = 0;
        } else {
            dht_error_count++;
            ESP_LOGW(TAG, "DHT read failed (%d consecutive errors)", dht_error_count);
        }

        // Logging inteligent (doar la schimbare de status sau la interval)
        if (count_log_time <= 0) {
            if (dht_valid_reading) {
                sensor_log_reading(SENSOR_DHT, dht_temp, dht_humid, true);
            } 
            else if (dht_error_count >= DHT_MAX_CONSECUTIVE_ERRORS) {
                sensor_log_reading(SENSOR_DHT, 0, 0, false);
            }

            count_log_time = DHT_DATA_LOG_INTERVAL;
        }
        
        count_log_time--;
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(DHT_SENSOR_DELAY_MS));
    }
}

esp_err_t start_dht_task(void)
{
    xTaskCreate(read_dht_task, "dht_task", 2048*2, NULL, 2, &DHT_TaskHandle);
    if (DHT_TaskHandle == NULL) return ESP_FAIL;

    sensor_register_dht();
    return ESP_OK;
}