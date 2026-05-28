#pragma once

#include "esp_err.h"
#include "event_bus.h"

// #define PCF8574_OUTPUT_ADDR     0x20
// #define PCF8574_INPUT_ADDR      0x21

// typedef enum {
//     EVENT_PCF_INPUT_CHANGED = 0x300,
//     EVENT_PCF_OUTPUT_CHANGED,
//     EVENT_BUTTON_PRESSED,
//     EVENT_FLOAT_SWITCH_CHANGED
// } pcf_event_id_t;

esp_err_t pcf_manager_init(void);

esp_err_t pcf_write(uint8_t pcf_addr, uint8_t value);
esp_err_t pcf_read(uint8_t pcf_addr, uint8_t *value);

esp_err_t pcf_set_output(uint8_t pin, bool state);
bool      pcf_get_input(uint8_t pin);

void pcf_manager_register_event_handler(esp_event_base_t handler);