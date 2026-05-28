#include "state_manager.h"
#include "time_manager.h"
#include "storage_manager.h"
#include "event_bus.h"
#include "buzzer.h"
#include "config.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "STATE_MGR";

static struct {
    system_state_t current_state;
    bool buzzer_muted;
    bool tank_low_flag;
    bool temp_error_flag;
} sys_state = {0};

// ====================== INTERNAL ======================
static const char* get_state_name(system_state_t state)
{
    switch (state) {
        case SYS_STATE_INIT:        return "INIT";
        case SYS_STATE_NORMAL:      return "NORMAL";
        case SYS_STATE_ALARM:       return "ALARM";
        case SYS_STATE_MAINTENANCE: return "MAINTENANCE";
        case SYS_STATE_ERROR:       return "ERROR";
        case SYS_STATE_SHUTDOWN:    return "SHUTDOWN";
        default:                    return "UNKNOWN";
    }
}

static void log_system_event(const char* event_type, const char* details)
{
    const char* time_str = time_manager_get_formatted_time();
    storage_log_printf(LOG_TYPE_SYSTEM, "[%s] %s | %s", 
                      time_str, event_type, details ? details : "-");
}

// ====================== EVENT HANDLER ======================
static void state_event_handler(void* handler_arg, 
                                esp_event_base_t base, 
                                int32_t event_id, 
                                void* event_data)
{
    (void)handler_arg;
    (void)event_data;

    // ==================== SYSTEM EVENTS ====================
    if (base == SYSTEM_EVENTS) {
        switch (event_id) {
            case EVENT_SYSTEM_STATE_CHANGED:
                // Aici poți procesa schimbarea de stare dacă vrei
                // state_change_event_t *data = (state_change_event_t*)event_data;
                break;

            case EVENT_SYSTEM_REBOOT_REQUESTED:
                ESP_LOGW(TAG, "Reboot requested by system event");
                // esp_restart(); sau deferred reboot
                break;

            default:
                break;
        }
    }

    // ==================== PCF EVENTS ====================
    else if (base == PCF_EVENTS) {
        switch (event_id) {
            case EVENT_BUTTON_PRESSED:
            case EVENT_PCF_INPUT_CHANGED:
                state_manager_handle_button_press();
                break;

            case EVENT_FLOAT_SWITCH_TRIGGERED:
                // Poți trata separat float switch dacă vrei
                break;

            default:
                break;
        }
    }

    // ==================== WIFI EVENTS ====================
    else if (base == WIFI_EVENTS) {
        switch (event_id) {
            case EVENT_WIFI_STA_CONNECTED:
                if (state_manager_is_in_maintenance()) {
                    state_manager_set_state(SYS_STATE_NORMAL, "WiFi connected - exit maintenance");
                }
                break;

            case EVENT_WIFI_STA_DISCONNECTED:
                // Poți face acțiuni la disconnect dacă vrei
                break;

            case EVENT_WIFI_STA_FAILED:
                ESP_LOGW(TAG, "WiFi connection failed");
                break;

            default:
                break;
        }
    }
}

// ====================== INIT ======================
esp_err_t state_manager_init(void)
{
    sys_state.current_state = SYS_STATE_INIT;
    sys_state.buzzer_muted = false;
    sys_state.tank_low_flag = false;
    sys_state.temp_error_flag = false;

    ESP_LOGI(TAG, "State Manager initialized");
    log_system_event("SYSTEM_BOOT", "Device powered on");

    return ESP_OK;
}

void state_manager_register_event_handlers(void)
{
    event_bus_register_handler(SYSTEM_EVENTS, ESP_EVENT_ANY_ID, state_event_handler, NULL);
    event_bus_register_handler(PCF_EVENTS,    ESP_EVENT_ANY_ID, state_event_handler, NULL);
    event_bus_register_handler(WIFI_EVENTS,   ESP_EVENT_ANY_ID, state_event_handler, NULL);

    ESP_LOGI(TAG, "State Manager registered to all relevant event bases");
}

// ====================== CORE FUNCTIONS ======================
esp_err_t state_manager_set_state(system_state_t new_state, const char* reason)
{
    system_state_t old_state = sys_state.current_state;
    if (old_state == new_state) return ESP_OK;

    sys_state.current_state = new_state;

    state_change_event_t event_data = {
        .old_state = old_state,
        .new_state = new_state,
        .reason = reason
    };

    log_system_event("STATE_CHANGED", reason);
    event_bus_post(SYSTEM_EVENTS, EVENT_SYSTEM_STATE_CHANGED, &event_data, sizeof(event_data));

    ESP_LOGI(TAG, "State changed: %s → %s | %s", 
             get_state_name(old_state), get_state_name(new_state), reason ? reason : "no reason");

    return ESP_OK;
}

system_state_t state_manager_get_state(void)
{
    return sys_state.current_state;
}

const char* state_manager_get_state_name(system_state_t state)
{
    return get_state_name(state);
}

// ====================== BUTTON & SENSOR ======================
void state_manager_handle_button_press(void)
{
    log_system_event("BUTTON_PRESSED", "Physical button");

    if (sys_state.current_state == SYS_STATE_ALARM) {
        sys_state.buzzer_muted = true;
        log_system_event("ALARM_MUTED", "By user button");
        return;
    }

    if (sys_state.current_state == SYS_STATE_NORMAL) {
        state_manager_set_state(SYS_STATE_MAINTENANCE, "Button: Enter maintenance");
    } else if (sys_state.current_state == SYS_STATE_MAINTENANCE) {
        state_manager_set_state(SYS_STATE_NORMAL, "Button: Exit maintenance");
        state_manager_clear_all_alarms();
    }
}

void state_manager_update_sensor_status(bool tank_low, bool temp_error)
{
    sys_state.tank_low_flag = tank_low;
    sys_state.temp_error_flag = temp_error;

    if (sys_state.current_state == SYS_STATE_MAINTENANCE) 
        return;

    if (tank_low || temp_error) {
        if (sys_state.current_state != SYS_STATE_ALARM) {
            state_manager_set_state(SYS_STATE_ALARM, "Sensor alarm triggered");
        }
    } else if (sys_state.current_state == SYS_STATE_ALARM) {
        state_manager_set_state(SYS_STATE_NORMAL, "All sensors returned to normal");
    }
}

// ====================== ALARMS ======================
esp_err_t state_manager_trigger_alarm(const char* reason)
{
    if (sys_state.current_state != SYS_STATE_ALARM) {
        state_manager_set_state(SYS_STATE_ALARM, reason);
    }
    return ESP_OK;
}

esp_err_t state_manager_clear_all_alarms(void)
{
    sys_state.buzzer_muted = false;
    sys_state.tank_low_flag = false;
    sys_state.temp_error_flag = false;
    log_system_event("ALARMS_CLEARED", "Manual clear");
    return ESP_OK;
}

bool state_manager_is_in_maintenance(void)
{
    return sys_state.current_state == SYS_STATE_MAINTENANCE;
}

bool state_manager_is_in_alarm(void)
{
    return sys_state.current_state == SYS_STATE_ALARM;
}




// #include "state_manager.h"
// #include "time_manager.h"
// #include "storage_manager.h"
// #include "buzzer.h"           // pentru pattern buzzer
// #include "config.h"

// #include <esp_log.h>
// #include <string.h>

// static const char *TAG = "STATE_MANAGER";


// // =============================================
// // INTERNAL LOGGING
// // =============================================
// static void log_system_event(const char* event_type, const char* details)
// {
//     struct tm tm = time_manager_get_current_time();
    
//     storage_log_printf(LOG_TYPE_SYSTEM, 
//         "\n%02u:%02u:%02u - %s | %s", 
//         tm.tm_hour, tm.tm_min, tm.tm_sec, 
//         event_type, details ? details : "no details");
// }


// static void on_event_received(event_id_t event, void* data, size_t data_size)
// {
//     switch (event) {
//         case EVENT_PCF_INPUT_CHANGED:
//             // Poți procesa butoane aici
//             state_manager_handle_button_press();
//             break;

//         case EVENT_WIFI_STA_CONNECTED:
//             // Exemplu: ieșim din maintenance la conectare WiFi
//             if (state_manager_is_in_maintenance()) {
//                 state_manager_set_state(SYS_STATE_NORMAL, "WiFi connected");
//             }
//             break;

//         case EVENT_TIME_UPDATED:
//             // Poți face acțiuni periodice bazate pe timp
//             break;

//         default:
//             break;
//     }
// }

// // =============================================
// // PUBLIC API
// // =============================================

// // ====================== REGISTER HANDLERS ======================
// void state_manager_register_event_handlers(void)
// {
//     event_bus_register_handler(SYSTEM_EVENTS, on_event_received);
//     event_bus_register_handler(PCF_EVENTS, on_event_received);
//     event_bus_register_handler(WIFI_EVENTS, on_event_received);
//     // adaugi și altele pe măsură ce apar
// }

// esp_err_t state_manager_init(void)
// {
//     sys_manager.current_state = SYS_STATE_INIT;
//     sys_manager.buzzer_muted = false;
//     sys_manager.tank_low_flag = false;
//     sys_manager.temp_error_flag = false;

//     ESP_LOGI(TAG, "State Manager initialized");
//     log_system_event("SYSTEM_START", "Device powered on");
    
//     return ESP_OK;
// }

// system_state_t state_manager_get_state(void)
// {
//     return sys_manager.current_state;
// }

// esp_err_t state_manager_set_state(system_state_t new_state, const char* reason)
// {
//     system_state_t old_state = sys_manager.current_state;
//     if (old_state == new_state) return ESP_OK;

//     sys_manager.current_state = new_state;

//     // Log + Event Bus
//     log_system_event("STATE_CHANGED", reason ? reason : "unknown");
    
//     state_change_event_t event_data = {
//         .old_state = old_state,
//         .new_state = new_state,
//         .reason = reason
//     };
    
//     event_bus_post(SYSTEM_EVENTS, EVENT_SYSTEM_STATE_CHANGED, &event_data, sizeof(event_data));

//     ESP_LOGI(TAG, "State changed: %d → %d | %s", old_state, new_state, reason ? reason : "");

//     return ESP_OK;
// }

// void state_manager_handle_button_press(void)
// {
//     log_system_event("BUTTON_PRESSED", "Physical button action");

//     if (sys_manager.current_state == SYS_STATE_ALARM) {
//         sys_manager.buzzer_muted = true;
//         log_system_event("ALARM_MUTED", "By physical button");
//         return;
//     }

//     // OLD TESTS
//     // Toggle NORMAL ↔ MAINTENANCE
//     if (sys_manager.current_state == SYS_STATE_NORMAL) {
//         state_manager_set_state(SYS_STATE_MAINTENANCE, "Button: Enter Maintenance");
//     } 
//     else if (sys_manager.current_state == SYS_STATE_MAINTENANCE) {
//         state_manager_set_state(SYS_STATE_NORMAL, "Button: Exit Maintenance");
//         state_manager_clear_all_alarms();
//     }
// }

// void state_manager_update_sensor_status(bool tank_low, bool temp_error)
// {
//     sys_manager.tank_low_flag = tank_low;
//     sys_manager.temp_error_flag = temp_error;

//     if (sys_manager.current_state == SYS_STATE_MAINTENANCE) 
//         return;   // Nu schimbăm stare în mentenanță

//     if (tank_low || temp_error) {
//         if (sys_manager.current_state != SYS_STATE_ALARM) {
//             state_manager_set_state(SYS_STATE_ALARM, "Sensor triggered alarm");
//         }
//     } else if (sys_manager.current_state == SYS_STATE_ALARM) {
//         state_manager_set_state(SYS_STATE_NORMAL, "Sensors returned to normal");
//     }
// }

// esp_err_t state_manager_trigger_alarm(const char* reason)
// {
//     if (sys_manager.current_state != SYS_STATE_ALARM) {
//         state_manager_set_state(SYS_STATE_ALARM, reason);
//     }
//     return ESP_OK;
// }

// esp_err_t state_manager_clear_all_alarms(void)
// {
//     sys_manager.buzzer_muted = false;
//     sys_manager.tank_low_flag = false;
//     sys_manager.temp_error_flag = false;
//     log_system_event("ALARMS_CLEARED", "All alarms reset");
//     return ESP_OK;
// }

// bool state_manager_is_in_maintenance(void)
// {
//     return sys_manager.current_state == SYS_STATE_MAINTENANCE;
// }

// bool state_manager_is_in_alarm(void)
// {
//     return sys_manager.current_state == SYS_STATE_ALARM;
// }

