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
    struct tm tm = time_manager_get_tm();
    
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



















// #include "state_manager.h"
// #include "time_manager.h"
// #include "buzzer.h"           // pentru pattern buzzer
// #include <esp_log.h>
// #include <string.h>

// static const char *TAG = "STATE_MANAGER";

// // state_manager_t sys_manager = {0};

// // Internal logging helper
// static void log_state_transition(system_state_t old_state, system_state_t new_state, const char* reason)
// {
//     char log_msg[128];
//     const char* state_names[] = {"INIT", "NORMAL", "MAINTENANCE", "ALARM", "ERROR"};

//     snprintf(log_msg, sizeof(log_msg), "STATE: %s -> %s | Reason: %s", 
//              state_names[old_state], state_names[new_state], reason ? reason : "unknown");

//     storage_log_printf(LOG_TYPE_SYSTEM, "\n%02u:%02u:%02u - %s", 
//                       rtc_time.tm_hour, rtc_time.tm_min, rtc_time.tm_sec, log_msg);

//     // Publicăm și pe event bus
//     state_change_event_t event = {
//         .old_state = old_state,
//         .new_state = new_state,
//         .reason = reason
//     };
//     event_bus_post(SYSTEM_EVENTS, SYS_EVENT_STATE_CHANGED, &event, sizeof(event));
// }

// // =============================================
// // PUBLIC API
// // =============================================

// esp_err_t state_manager_init(void)
// {
//     sys_manager.current_state = SYS_STATE_INIT;
//     sys_manager.buzzer_muted = false;
//     sys_manager.tank_low_flag = false;
//     sys_manager.temp_error_flag = false;

//     ESP_LOGI(TAG, "State Manager initialized (INIT state)");
//     storage_log_printf(LOG_TYPE_SYSTEM, "\n%02u:%02u:%02u - SYSTEM STARTED", 
//                       rtc_time.tm_hour, rtc_time.tm_min, rtc_time.tm_sec);
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

//     ESP_LOGI(TAG, "State transition: %d -> %d | %s", old_state, new_state, reason ? reason : "");

//     sys_manager.current_state = new_state;
//     log_state_transition(old_state, new_state, reason);

//     return ESP_OK;
// }

// void state_manager_handle_button_press(void)
// {
//     ESP_LOGI(TAG, "Physical button pressed");

//     if (sys_manager.current_state == SYS_STATE_ALARM) {
//         sys_manager.buzzer_muted = true;
//         // buzzer_stop_all_alarms();  // vei apela din event listener
//         ESP_LOGI(TAG, "Alarm muted via physical button");
//         return;
//     }

//     // Toggle NORMAL ↔ MAINTENANCE
//     if (sys_manager.current_state == SYS_STATE_NORMAL) {
//         state_manager_set_state(SYS_STATE_MAINTENANCE, "Button pressed");
//     } else if (sys_manager.current_state == SYS_STATE_MAINTENANCE) {
//         state_manager_set_state(SYS_STATE_NORMAL, "Button pressed - exit maintenance");
//         state_manager_clear_all_alarms();
//     }
// }

// void state_manager_update_sensor_status(bool tank_low, bool temp_error)
// {
//     sys_manager.tank_low_flag = tank_low;
//     sys_manager.temp_error_flag = temp_error;

//     if (sys_manager.current_state == SYS_STATE_MAINTENANCE) 
//         return; // Nu schimbăm stare în timpul mentenanței

//     if (tank_low || temp_error) {
//         if (sys_manager.current_state != SYS_STATE_ALARM) {
//             state_manager_set_state(SYS_STATE_ALARM, "Sensor alarm triggered");
//         }
//     } else if (sys_manager.current_state == SYS_STATE_ALARM) {
//         state_manager_set_state(SYS_STATE_NORMAL, "Sensors returned to normal");
//     }
// }

// bool state_manager_is_in_maintenance(void)
// {
//     return sys_manager.current_state == SYS_STATE_MAINTENANCE;
// }

// bool state_manager_is_in_alarm(void)
// {
//     return sys_manager.current_state == SYS_STATE_ALARM;
// }

// // ==================== NEW FUNCTIONS ====================
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
//     ESP_LOGI(TAG, "All alarms cleared");
//     return ESP_OK;
// }

// void state_manager_log_event(const char* event, const char* details)
// {
//     storage_log_printf(LOG_TYPE_SYSTEM, "\n%02u:%02u:%02u - %s | %s", 
//                       rtc_time.tm_hour, rtc_time.tm_min, rtc_time.tm_sec,
//                       event, details ? details : "");
// }













// #include "state_manager.h"
// #include "esp_log.h"
// #include "buzzer.h"
// // #include "lcd_display.h" // Îl deblocăm când actualizăm ecranul

// static const char *TAG = "STATE_MANAGER";


// // Inițializarea stării la pornirea plăcii ESP32
// void state_manager_init(void) {
//     sys_manager.current_state = SYS_STATE_INIT;
//     sys_manager.buzzer_muted = false;
//     sys_manager.tank_low_flag = false;
//     sys_manager.temp_error_flag = false;
//     ESP_LOGI(TAG, "State Manager inițializat în starea INIT.");
// }

// // Returnează starea curentă a sistemului
// system_state_t state_manager_get_state(void) {
//     return sys_manager.current_state;
// }

// // Schimbă starea sistemului și execută acțiunile hardware de tranziție
// void state_manager_set_state(system_state_t new_state) {
//     if (sys_manager.current_state == new_state) return; // Starea nu s-a schimbat

//     ESP_LOGI(TAG, "Tranziție stare: %d ---> %d", sys_manager.current_state, new_state);
//     sys_manager.current_state = new_state;

//     switch (new_state) {
//         case SYS_STATE_NORMAL:
//             sys_manager.buzzer_muted = false; // Resetăm starea de mute la revenirea în normal
//             buzzer_stop_all_alarms(); // Oprim orice sunet restant
//             ESP_LOGI(TAG, "Sistemul rulează în regim automat normal.");
//             break;

//         case SYS_STATE_MAINTENANCE:
//             buzzer_stop_all_alarms();
//             // Aici pornești acel beep scurt/discret de șantier (mentenanță) la 3 secunde
//             // buzzer_trigger_alarm(ALARMA_BEEP_MENTENANTA);
//             ESP_LOGW(TAG, "Sistemul a intrat în MENTENANȚĂ (Schimb Apă). Pompele trebuie blocate!");
//             break;

//         case SYS_STATE_ALARM:
//             // Activăm buzzerul doar dacă utilizatorul nu a apăsat deja Mute anterior
//             if (!sys_manager.buzzer_muted) {
//                 // buzzer_trigger_alarm(ALARMA_REZERVOR_GOL);
//             }
//             ESP_LOGE(TAG, "ALARMĂ ACTIVĂ! Verifică starea critică a componentelor hardware.");
//             break;

//         default:
//             break;
//     }
// }

// // Logica unificată a butonului fizic unic (Ordine de Priorități)
// void state_manager_handle_button_press(void) {
//     ESP_LOGI(TAG, "Buton fizic detectat! Procesare acțiune...");

//     // PRIORITATEA 1: Dacă suntem în starea de ALARMĂ, prima apăsare oprește sunetul (Mute)
//     if (sys_manager.current_state == SYS_STATE_ALARM) {
//         if (!sys_manager.buzzer_muted) {
//             sys_manager.buzzer_muted = true;
//             buzzer_stop_all_alarms(); // Dezactivăm acustic buzzerul, dar starea rămâne ALARM pe ecran
//             ESP_LOGI(TAG, "Alarmă pusă pe MUTE de la butonul fizic.");
            
//             // Trimitem și paginii web starea prin WebSocket (opțional, când legăm ws)
//             // ws_send_single_update("alarm_mute", "1");
//         }
//         return;
//     }

//     // PRIORITATEA 2: Dacă sistemul este în stare normală sau mentenanță, butonul face TOGGLE
//     if (sys_manager.current_state == SYS_STATE_NORMAL) {
//         state_manager_set_state(SYS_STATE_MAINTENANCE);
//     } 
//     else if (sys_manager.current_state == SYS_STATE_MAINTENANCE) {
//         // La ieșirea din mentenanță, verificăm dacă nu cumva s-a golit bazinul între timp
//         if (sys_manager.tank_low_flag || sys_manager.temp_error_flag) {
//             state_manager_set_state(SYS_STATE_ALARM);
//         } else {
//             state_manager_set_state(SYS_STATE_NORMAL);
//         }
//     }
// }

// // Actualizează indicatorii de senzori trimiși de task-urile de citire hardware
// void state_manager_update_sensors_status(bool tank_low, bool temp_out_of_bounds) {
//     sys_manager.tank_low_flag = tank_low;
//     sys_manager.temp_error_flag = temp_out_of_bounds;

//     // Logica de auto-Tranziție bazată pe senzori
//     if (sys_manager.current_state != SYS_STATE_MAINTENANCE && sys_manager.current_state != SYS_STATE_INIT) {
//         if (tank_low || temp_out_of_bounds) {
//             state_manager_set_state(SYS_STATE_ALARM);
//         } else if (sys_manager.current_state == SYS_STATE_ALARM) {
//             // Dacă erorile au dispărut (ai umplut bazinul), sistemul revine automat în normal
//             state_manager_set_state(SYS_STATE_NORMAL);
//         }
//     }
// }
