#pragma once

#include "esp_err.h"
#include "event_bus.h"
#include "nvs_manager.h"
#include "config.h"
#include <stdbool.h>
#include <time.h>


typedef struct {
    system_state_t old_state;
    system_state_t new_state;
    const char* reason;
} state_change_event_t;

// ==================== PUBLIC API ====================
esp_err_t state_manager_init(void);

system_state_t state_manager_get_state(void);
esp_err_t state_manager_set_state(system_state_t new_state, const char* reason);

void state_manager_handle_button_press(void);
void state_manager_update_sensor_status(bool tank_low, bool temp_error);

esp_err_t state_manager_trigger_alarm(const char* reason);
esp_err_t state_manager_clear_all_alarms(void);

bool state_manager_is_in_maintenance(void);
bool state_manager_is_in_alarm(void);

// Event Bus integration
void state_manager_register_event_handlers(void);

// Pentru debug / logging
const char* state_manager_get_state_name(system_state_t state);