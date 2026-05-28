#pragma once

#include "esp_event.h"
#include <time.h>
#include <stdint.h>

// =============================================
// EVENT BASES
// =============================================
ESP_EVENT_DECLARE_BASE(SYSTEM_EVENTS);
ESP_EVENT_DECLARE_BASE(SENSOR_EVENTS);
ESP_EVENT_DECLARE_BASE(ENVIRONMENT_EVENTS);
ESP_EVENT_DECLARE_BASE(WIFI_EVENTS);
ESP_EVENT_DECLARE_BASE(PCF_EVENTS);
ESP_EVENT_DECLARE_BASE(HYDRO_EVENTS);
ESP_EVENT_DECLARE_BASE(LOG_EVENTS);

// =============================================
// SYSTEM EVENTS
// =============================================
typedef enum {
    SYS_EVENT_TIME_UPDATED = 0,
    SYS_EVENT_TIME_SYNCED,
    SYS_EVENT_REBOOT_REQUESTED,
    SYS_EVENT_STATE_CHANGED,
    SYS_EVENT_BUTTON_PRESSED,
    SYS_EVENT_SENSOR_UPDATE,
    SYS_EVENT_ALARM_TRIGGERED,
    SYS_EVENT_MAINTENANCE_START,
    SYS_EVENT_MAINTENANCE_END,
} system_event_id_t;

// =============================================
// WIFI EVENTS
// =============================================
typedef enum {
    EVENT_WIFI_STATE_CHANGED = 0x200,
    EVENT_WIFI_STA_CONNECTED,
    EVENT_WIFI_STA_DISCONNECTED,
    EVENT_WIFI_STA_FAILED,
    EVENT_WIFI_AP_STOPPED,
    EVENT_WIFI_FALLBACK_STARTED
} wifi_event_id_t;

// =============================================
// PCF EVENTS
// =============================================

typedef enum {
    EVENT_PCF_INPUT_CHANGED = 0x300,
    EVENT_PCF_OUTPUT_CHANGED,
    EVENT_BUTTON_PRESSED,
    EVENT_FLOAT_SWITCH_CHANGED
} pcf_event_id_t;

// =============================================
// DATA STRUCTURES
// =============================================
typedef struct {
    struct tm timeinfo;
    time_t unix_time;
    bool is_synced;
} time_event_data_t;

// =============================================
// PUBLIC API
// =============================================
esp_err_t event_bus_init(void);

esp_err_t event_bus_post(esp_event_base_t base, int32_t event_id, 
                        void *event_data, size_t event_data_size);

esp_err_t event_bus_register_handler(esp_event_base_t base, int32_t event_id,
                                    esp_event_handler_t handler, void *handler_arg);

esp_err_t event_bus_unregister_handler(esp_event_base_t base, int32_t event_id,
                                      esp_event_handler_t handler);