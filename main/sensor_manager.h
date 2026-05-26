#pragma once

#include <esp_err.h>
#include "event_bus.h"
#include "storage_manager.h"

// ==================== SENSOR TYPES ====================
typedef enum {
    SENSOR_DHT = 0,
    SENSOR_DS18B20,
} sensor_type_t;

// ==================== PUBLIC API ====================
esp_err_t sensor_manager_init(void);

// Register sensors (called from their init tasks)
esp_err_t sensor_register_dht(void);
esp_err_t sensor_register_ds18b20(void);

// Common logging function
esp_err_t sensor_log_reading(sensor_type_t type, float temp, float humidity, bool valid);

// Status
bool sensor_manager_is_ready(void);
