#pragma once

#include <esp_err.h>
#include <time.h>
#include "time_manager.h"
#include "event_bus.h"
#include <stdbool.h>

// =============================================
// CONFIGURATION
// =============================================
#define MOUNT_POINT_SD      "/sdcard"
#define MOUNT_POINT_LOGS    MOUNT_POINT_SD "/logs"
#define MOUNT_POINT_INFO    MOUNT_POINT_SD "/info"
#define MOUNT_POINT_ERRORS  MOUNT_POINT_SD "/errors"

#define LOG_MAXDELAY        512
#define LOG_DATA_QUEUE_SIZE 15

typedef enum {
    LOG_TYPE_GENERAL = 0,
    LOG_TYPE_RESET,
    LOG_TYPE_DHT,
    LOG_TYPE_DS18B20,
    LOG_TYPE_ERROR,
    LOG_TYPE_SYSTEM
} log_type_t;

// =============================================
// PUBLIC API
// =============================================
esp_err_t storage_manager_init(void);

esp_err_t storage_log_write(log_type_t type, const char *data, size_t len);
esp_err_t storage_log_printf(log_type_t type, const char *format, ...);

bool storage_is_mounted(void);

// esp_err_t storage_log_sensor_data(log_type_t type, const char *data);

// esp_err_t storage_get_free_space(uint64_t *bytes_free);

// Legacy compatibility (temporar)
extern const char log_types[7][7];

// =============================================
// EVENT SUPPORT
// =============================================
// esp_err_t storage_manager_post_log_event(log_type_t type, const char *message);