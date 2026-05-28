#pragma once

#include "esp_err.h"
#include "event_bus.h"

esp_err_t pcf_manager_init(void);

esp_err_t pcf_write(uint8_t pcf_addr, uint8_t value);
esp_err_t pcf_read(uint8_t pcf_addr, uint8_t *value);

esp_err_t pcf_set_output(uint8_t pin, bool state);
bool      pcf_get_input(uint8_t pin);

void pcf_manager_register_event_handler(esp_event_base_t handler);