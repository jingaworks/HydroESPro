#pragma once

#include <stdbool.h>
#include <esp_err.h>
#include <board.h>
#include "event_bus.h"
#include "storage_manager.h"


// ==================== PUBLIC API ====================
esp_err_t state_manager_init(void);

system_state_t state_manager_get_state(void);
esp_err_t state_manager_set_state(system_state_t new_state, const char* reason);

void state_manager_handle_button_press(void);
void state_manager_update_sensor_status(bool tank_low, bool temp_error);

bool state_manager_is_in_maintenance(void);
bool state_manager_is_in_alarm(void);

// New functions (Etapa 1)
esp_err_t state_manager_trigger_alarm(const char* reason);
esp_err_t state_manager_clear_all_alarms(void);
void state_manager_log_event(const char* event, const char* details);

// Legacy global (păstrat pentru compatibilitate)
// extern state_manager_t sys_manager;

// #ifndef STATE_MANAGER_H
// #define STATE_MANAGER_H

// #include <stdbool.h>
// #include "board.h"


// // Prototipurile funcțiilor de control
// void state_manager_init(void);
// system_state_t state_manager_get_state(void);
// void state_manager_set_state(system_state_t new_state);
// void state_manager_handle_button_press(void);
// void state_manager_update_sensors_status(bool tank_low, bool temp_out_of_bounds);

// #endif // STATE_MANAGER_H
