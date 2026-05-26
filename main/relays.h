#pragma once

#include <esp_err.h>
#include <stdbool.h>

// ==================== PUBLIC API (simplu) ====================
void switch_relay(int relay_gpio, bool state);

// ==================== EVENT-DRIVEN API ====================
esp_err_t relays_init(void);

// Comenzi prin event bus (recomandat pentru nou cod)
void relays_set_water_pump(bool on);
void relays_set_air_pump(bool on);




// #pragma once

// #include <esp_err.h>
// #include <esp_log.h>
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>
// #include "freertos/timers.h"
// #include <driver/gpio.h>


// void switch_relay(int relay, bool state);













// // // #include "pins_pcf8574.h"


// // #define NUMBER_OF_RELAYS 8


// // extern TimerHandle_t xTimerCheckRelaysState;



// // bool relays_status[NUMBER_OF_RELAYS];


// // esp_err_t switch_relays_start_task(void);

// // void vTimerCheckRelaysState(TimerHandle_t pxTimer);
