#include "state_manager.h"
#include "time_manager.h"
#include "storage_manager.h"
#include "buzzer.h"           // pentru pattern buzzer
#include <esp_log.h>
#include <string.h>

static const char *TAG = "STATE_MANAGER";


// =============================================
// INTERNAL LOGGING
// =============================================
static void log_system_event(const char* event_type, const char* details)
{
    struct tm tm = time_manager_get_current_time();
    
    storage_log_printf(LOG_TYPE_SYSTEM, 
        "\n%02u:%02u:%02u - %s | %s", 
        tm.tm_hour, tm.tm_min, tm.tm_sec, 
        event_type, details ? details : "no details");
}

// =============================================
// PUBLIC API
// =============================================

esp_err_t state_manager_init(void)
{
    sys_manager.current_state = SYS_STATE_INIT;
    sys_manager.buzzer_muted = false;
    sys_manager.tank_low_flag = false;
    sys_manager.temp_error_flag = false;

    ESP_LOGI(TAG, "State Manager initialized");
    log_system_event("SYSTEM_START", "Device powered on");
    
    return ESP_OK;
}

system_state_t state_manager_get_state(void)
{
    return sys_manager.current_state;
}

esp_err_t state_manager_set_state(system_state_t new_state, const char* reason)
{
    system_state_t old_state = sys_manager.current_state;
    if (old_state == new_state) return ESP_OK;

    sys_manager.current_state = new_state;

    // Log + Event Bus
    log_system_event("STATE_CHANGED", reason ? reason : "unknown");
    
    state_change_event_t event_data = {
        .old_state = old_state,
        .new_state = new_state,
        .reason = reason
    };
    
    event_bus_post(SYSTEM_EVENTS, SYS_EVENT_STATE_CHANGED, &event_data, sizeof(event_data));

    ESP_LOGI(TAG, "State changed: %d → %d | %s", old_state, new_state, reason ? reason : "");

    return ESP_OK;
}

void state_manager_handle_button_press(void)
{
    log_system_event("BUTTON_PRESSED", "Physical button action");

    if (sys_manager.current_state == SYS_STATE_ALARM) {
        sys_manager.buzzer_muted = true;
        log_system_event("ALARM_MUTED", "By physical button");
        return;
    }

    // Toggle NORMAL ↔ MAINTENANCE
    if (sys_manager.current_state == SYS_STATE_NORMAL) {
        state_manager_set_state(SYS_STATE_MAINTENANCE, "Button: Enter Maintenance");
    } 
    else if (sys_manager.current_state == SYS_STATE_MAINTENANCE) {
        state_manager_set_state(SYS_STATE_NORMAL, "Button: Exit Maintenance");
        state_manager_clear_all_alarms();
    }
}

void state_manager_update_sensor_status(bool tank_low, bool temp_error)
{
    sys_manager.tank_low_flag = tank_low;
    sys_manager.temp_error_flag = temp_error;

    if (sys_manager.current_state == SYS_STATE_MAINTENANCE) 
        return;   // Nu schimbăm stare în mentenanță

    if (tank_low || temp_error) {
        if (sys_manager.current_state != SYS_STATE_ALARM) {
            state_manager_set_state(SYS_STATE_ALARM, "Sensor triggered alarm");
        }
    } else if (sys_manager.current_state == SYS_STATE_ALARM) {
        state_manager_set_state(SYS_STATE_NORMAL, "Sensors returned to normal");
    }
}

esp_err_t state_manager_trigger_alarm(const char* reason)
{
    if (sys_manager.current_state != SYS_STATE_ALARM) {
        state_manager_set_state(SYS_STATE_ALARM, reason);
    }
    return ESP_OK;
}

esp_err_t state_manager_clear_all_alarms(void)
{
    sys_manager.buzzer_muted = false;
    sys_manager.tank_low_flag = false;
    sys_manager.temp_error_flag = false;
    log_system_event("ALARMS_CLEARED", "All alarms reset");
    return ESP_OK;
}

bool state_manager_is_in_maintenance(void)
{
    return sys_manager.current_state == SYS_STATE_MAINTENANCE;
}

bool state_manager_is_in_alarm(void)
{
    return sys_manager.current_state == SYS_STATE_ALARM;
}

