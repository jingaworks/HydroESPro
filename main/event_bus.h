#pragma once

#include "esp_event.h"
#include <time.h>
#include <stdint.h>
#include "esp_event_base.h"
#include "esp_err.h"

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

#pragma once

// =============================================
//                  BASE EVENTS
// =============================================
typedef enum {
    // System wide events
    EVENT_SYSTEM_BOOT_COMPLETE = 0x0000,
    EVENT_SYSTEM_REBOOT_REQUESTED,
    EVENT_SYSTEM_STATE_CHANGED,
    EVENT_SYSTEM_ERROR,
    EVENT_SYSTEM_MAINTENANCE_START,
    EVENT_SYSTEM_MAINTENANCE_END,

    // Time related
    EVENT_TIME_UPDATED = 0x0100,
    EVENT_TIME_SNTP_SYNC_SUCCESS,
    EVENT_TIME_SNTP_SYNC_FAILED,

    // WiFi
    EVENT_WIFI_STATE_CHANGED = 0x0200,
    EVENT_WIFI_STA_CONNECTED,
    EVENT_WIFI_STA_DISCONNECTED,
    EVENT_WIFI_STA_FAILED,
    EVENT_WIFI_AP_STARTED,
    EVENT_WIFI_AP_STOPPED,
    EVENT_WIFI_SCAN_DONE,

    // PCF / Inputs
    EVENT_PCF_INPUT_CHANGED = 0x0300,
    EVENT_PCF_OUTPUT_CHANGED,
    EVENT_BUTTON_PRESSED,
    EVENT_FLOAT_SWITCH_TRIGGERED,

    // Hydroponics
    EVENT_HYDRO_PH_CHANGED = 0x0400,
    EVENT_HYDRO_EC_CHANGED,
    EVENT_HYDRO_TEMPERATURE_CHANGED,
    EVENT_HYDRO_PUMP_STARTED,
    EVENT_HYDRO_PUMP_STOPPED,

    // Alarm
    EVENT_ALARM_TRIGGERED = 0x0600,

    // Future modules
    EVENT_LIGHT_STATE_CHANGED = 0x0600,
    EVENT_AIR_SYSTEM_CHANGED  = 0x0700,
    
    // ...

} event_id_t;

// Handler type
typedef void (*event_handler_t)(void* handler_arg, esp_event_base_t base, 
                                int32_t event_id, void* event_data);

// =============================================
// PUBLIC API
// =============================================
esp_err_t event_bus_init(void);

esp_err_t event_bus_post(esp_event_base_t base, int32_t event_id, 
                        void *event_data, size_t event_data_size);

esp_err_t event_bus_register_handler(esp_event_base_t base, int32_t event_id,
                                    esp_event_handler_t handler, void *handler_arg);
