#pragma once

#include <esp_log.h>
#include <esp_event.h>
#include <time.h>
#include <stdint.h>

// =============================================
// EVENT BASES
// =============================================
ESP_EVENT_DECLARE_BASE(SYSTEM_EVENTS);
ESP_EVENT_DECLARE_BASE(SENSOR_EVENTS);
ESP_EVENT_DECLARE_BASE(ENVIRONMENT_EVENTS);
ESP_EVENT_DECLARE_BASE(LOG_EVENTS);
ESP_EVENT_DECLARE_BASE(WIFI_EVENTS_EXT);

// =============================================
// SYSTEM EVENTS
// =============================================
typedef enum {
    SYS_EVENT_TIME_UPDATED = 0,        // Called periodically or on RTC update
    SYS_EVENT_TIME_SYNCED,             // SNTP / manual sync successful
    SYS_EVENT_REBOOT_REQUESTED,
    
    SYS_EVENT_STATE_CHANGED,
    SYS_EVENT_ALARM_TRIGGERED,
    SYS_EVENT_MAINTENANCE_START,
    SYS_EVENT_MAINTENANCE_END,
    SYS_EVENT_BUTTON_PRESSED,
    SYS_EVENT_SENSOR_UPDATE,
} system_event_id_t;

// =============================================
// SENSOR EVENTS
// =============================================
typedef enum {
    EVENT_DHT_READY = 0,
    EVENT_DS18B20_READY,
    EVENT_ALL_SENSORS_READY,
} sensor_event_id_t;

// =============================================
// ENVIRONMENT EVENTS
// =============================================
typedef enum {
    EVENT_RELAYS_STATE_CHANGED = 0,
    EVENT_STEALTH_MODE_CHANGED,
    EVENT_ENVIRONMENT_CONFIG_CHANGED,
} environment_event_id_t;

// =============================================
// DATA STRUCTURES FOR EVENTS
// =============================================
typedef struct {
    struct tm timeinfo;
    time_t unix_time;
    bool is_synced;
    bool is_dst;
} time_event_data_t;

typedef struct {
    float temperature;
    float humidity;
    bool valid;
} dht_event_data_t;

typedef struct {
    float temperature;
    bool valid;
    uint8_t sensor_index;
} ds_event_data_t;

// =============================================
// PUBLIC API
// =============================================
esp_err_t event_bus_init(void);

esp_err_t event_bus_post(esp_event_base_t base, int32_t event_id, 
                        void *event_data, size_t event_data_size);

esp_err_t event_bus_register_handler(esp_event_base_t base, 
                                    int32_t event_id,
                                    esp_event_handler_t handler, 
                                    void *handler_arg);

esp_err_t event_bus_unregister_handler(esp_event_base_t base, 
                                      int32_t event_id,
                                      esp_event_handler_t handler);