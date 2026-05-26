#include "ds18b20.h"
#include "buzzer.h"
#include "storage_manager.h"
#include "time_manager.h"
#include "sensor_manager.h"


static const char *TAG = "DS18B20";


// Definirea tipurilor de alarme conform cerințelor tale
const buzzer_alarm_t ALARMA_APA_CALDA = {
    .alarm_id = ID_APA_CALDA,
    .on_ms = 100, .off_ms = 3000, .beep_count = 3, .pause_ms = 10000,
    .total_cycles = 3,  // Va suna doar 3 cicluri, apoi se oprește singură!
    .priority = 2       // Prioritate mai mică (2)
};


TaskHandle_t DS18B20_TaskHandle = NULL;

ds_sensors_t ds_sensors;

// static const int RESCAN_INTERVAL = 5;
onewire_addr_t ds_address[MAX_DS_SENSORS];
float ds_temps[MAX_DS_SENSORS];


float ds_temp;


float ds_temp_alarm = 28.0;
bool alarm_active = false;

void check_temp_alarm(void) {
    if (ds_temps[0] >= ds_temp_alarm) {
        // if (alarm_active) return;
        if (buzzer_is_alarm_active(ID_APA_CALDA)) return;
        buzzer_trigger_alarm(ALARMA_APA_CALDA);
    }
    else if (ds_temps[0] < ds_temp_alarm) {
        if (!buzzer_is_alarm_active(ID_APA_CALDA)) return;
        buzzer_clear_alarm_by_id(ID_APA_CALDA);
    }
}


bool ds_valid_reding;

static void read_ds18b20_task(void *pvParameter) {
    TickType_t xLastWakeTime;
    size_t count_log_time = 0;
    size_t sensor_count = 0;
    esp_err_t ret;
    for (;;) {
        xLastWakeTime = xTaskGetTickCount();

        ret = ds18x20_scan_devices(SENSOR_DS_GPIO, ds_address, MAX_DS_SENSORS, &sensor_count);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Sensors scan error %d (%s)", ret, esp_err_to_name(ret));
            ds_valid_reding = false;
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(DS_SENSOR_DELAY_MS));
            continue;
        }

        if (!sensor_count) {
            ESP_LOGW(TAG, "No sensors detected!");
            ds_valid_reding = false;
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(DS_SENSOR_DELAY_MS));
            continue;
        }

        if (sensor_count > MAX_DS_SENSORS) sensor_count = MAX_DS_SENSORS;

        ret = ds18x20_measure_and_read_multi(SENSOR_DS_GPIO, ds_address, sensor_count, ds_temps);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ret %s", esp_err_to_name(ret));
            ESP_LOGE(TAG, "Could not read data from DS Sensors");
            ds_valid_reding = false;
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(DS_SENSOR_DELAY_MS));
            continue;
        }

        ds_sensors.sensors_num = sensor_count;

        for (int j = 0; j < sensor_count; j++) {
            ds_temp = ds_temps[j];

            ds_sensors.data[j].id = j + 1;
            ds_sensors.data[j].address = ds_address[j];
            ds_sensors.data[j].tempC = ds_temp;
            ds_sensors.data[j].read_time = get_time_sec();

            ds_valid_reding = true;
            // ESP_LOGI(TAG, "#%u Sensor %08" PRIx32 "%08" PRIx32 " (DS18B20) reports %.3f°C", j + 1,
            //         (uint32_t)(ds_address[j] >> 32), (uint32_t)ds_address[j], ds_temp);
        }

        if (ds_valid_reding && count_log_time == 0) {
            for (int j = 0; j < sensor_count; j++) {
                sensor_log_reading(SENSOR_DS18B20, ds_temps[j], 0.0f, true);
            }
            count_log_time = DS_DATA_LOG_INTERVAL;
        }

        count_log_time--;

        check_temp_alarm();

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(DS_SENSOR_DELAY_MS));
    }
}

esp_err_t start_ds18b20_task(void) {
    xTaskCreate(read_ds18b20_task, "raed ds task", 2048 * 2, NULL, 2, &DS18B20_TaskHandle);
    if (DS18B20_TaskHandle == NULL) return ESP_FAIL;
    return ESP_OK;
}
