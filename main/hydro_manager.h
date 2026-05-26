#pragma once

#include <esp_err.h>
#include "event_bus.h"

// ==================== PUBLIC API ====================
esp_err_t hydro_manager_init(void);
esp_err_t start_control_hydroponics_task(void);

// Status queries
bool hydro_is_water_pump_on(void);
bool hydro_is_air_pump_on(void);
