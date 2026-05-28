#include "relays.h"
#include "board.h"
#include "config.h"
#include "event_bus.h"
#include "state_manager.h"
#include "storage_manager.h"
#include "time_manager.h"
#include <esp_log.h>
#include <driver/gpio.h>


static const char *TAG = "RELAYS";

// =============================================
// EVENT HANDLER
// =============================================
static void relays_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    if (base != SYSTEM_EVENTS) return;

    switch (event_id) {
        case SYS_EVENT_STATE_CHANGED: {
            state_change_event_t *ev = (state_change_event_t *)event_data;

            if (ev->new_state == SYS_STATE_MAINTENANCE) {
                ESP_LOGW(TAG, "MAINTENANCE → Forcing all relays OFF");
                switch_relay(RELAY_WATER_PUMP_GPIO, false);
                switch_relay(RELAY_AIR_PUMP_GPIO, false);
                storage_log_printf(LOG_TYPE_SYSTEM, "\n%s - RELAYS: All OFF (Maintenance)", 
                                  time_manager_get_formatted_time());
            }
            break;
        }
    }
}

// =============================================
// LOW-LEVEL CONTROL
// =============================================
void switch_relay(int relay_gpio, bool state)
{
    
    // LED asociat
    if (relay_gpio == RELAY_WATER_PUMP_GPIO) {
        gpio_set_level(RELAY_WATER_PUMP_GPIO, state ? RELEU_PORNIT : RELEU_OPRIT);
        gpio_set_level(WATER_PUMP_LED_GPIO, state ? LED_PORNIT : LED_OPRIT);
    } else if (relay_gpio == RELAY_AIR_PUMP_GPIO) {
        gpio_set_level(RELAY_AIR_PUMP_GPIO, state ? RELEU_PORNIT : RELEU_OPRIT);
        gpio_set_level(AIR_PUMP_LED_GPIO, state ? LED_PORNIT : LED_OPRIT);
    }

    // Logging
    struct tm tm = time_manager_get_current_time();
    storage_log_printf(LOG_TYPE_SYSTEM, "\n%02u:%02u:%02u - RELAY %d = %s", 
                      tm.tm_hour, tm.tm_min, tm.tm_sec, 
                      relay_gpio, state ? "ON" : "OFF");
}

// =============================================
// EVENT-DRIVEN COMMANDS
// =============================================
void relays_set_water_pump(bool on)
{
    switch_relay(RELAY_WATER_PUMP_GPIO, on);
}

void relays_set_air_pump(bool on)
{
    switch_relay(RELAY_AIR_PUMP_GPIO, on);
}

// =============================================
// INIT
// =============================================
esp_err_t relays_init(void)
{
    // Configurare GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RELAY_WATER_PUMP_GPIO) | 
                        (1ULL << RELAY_AIR_PUMP_GPIO) |
                        (1ULL << WATER_PUMP_LED_GPIO) |
                        (1ULL << AIR_PUMP_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Stare inițială OFF
    switch_relay(RELAY_WATER_PUMP_GPIO, false);
    switch_relay(RELAY_AIR_PUMP_GPIO, false);

    // Abonare la evenimente
    event_bus_register_handler(SYSTEM_EVENTS, SYS_EVENT_STATE_CHANGED, 
                              relays_event_handler, NULL);

    ESP_LOGI(TAG, "Relays initialized with Event Bus");
    return ESP_OK;
}









// #include "relays.h"
// #include "config.h"



// static const char *TAG = "RELAYS";


// void turn_relay_on(int relay) {
//     if (relay == RELAY_WATER_PUMP_GPIO) {
//         // ESP_LOGI(TAG, "turn_on WATER PUMP");
//         gpio_set_level(RELAY_WATER_PUMP_GPIO, RELEU_PORNIT);
//         // ESP_LOGI(TAG, "turn_on WATER PUMP LED");
//         gpio_set_level(WATER_PUMP_LED_GPIO, LED_PORNIT);
//     }
//     else if (relay == RELAY_AIR_PUMP_GPIO) {
//         // ESP_LOGI(TAG, "turn_on AIR PUMP");
//         gpio_set_level(RELAY_AIR_PUMP_GPIO, RELEU_PORNIT);
//         // ESP_LOGI(TAG, "turn_on AIR PUMP LED");
//         gpio_set_level(AIR_PUMP_LED_GPIO, LED_PORNIT);
//     }
// }

// void turn_relay_off(int relay) {
//     if (relay == RELAY_WATER_PUMP_GPIO) {
//         // ESP_LOGI(TAG, "turn_off WATER PUMP");
//         gpio_set_level(RELAY_WATER_PUMP_GPIO, RELEU_OPRIT);
//         // ESP_LOGI(TAG, "turn_off WATER PUMP LED");
//         gpio_set_level(WATER_PUMP_LED_GPIO, LED_OPRIT);
//     }
//     else if (relay == RELAY_AIR_PUMP_GPIO) {
//         // ESP_LOGI(TAG, "turn_off AIR PUMP");
//         gpio_set_level(RELAY_AIR_PUMP_GPIO, RELEU_OPRIT);
//         // ESP_LOGI(TAG, "turn_off AIR PUMP LED");
//         gpio_set_level(AIR_PUMP_LED_GPIO, LED_OPRIT);
//     }

// }

// void switch_relay(int relay, bool state) {
//     if (state) {
//         turn_relay_on(relay);
//     }
//     else {
//         turn_relay_off(relay);
//     }
// }





























// // TaskHandle_t SWITCH_RELAYS_TaskHandle;

// // static bool last_relays_state[8] = {false};

// // bool relays_status[NUMBER_OF_RELAYS] = {0};


// // static void switch_relays_task(void *pvParameters)
// // {
// //     uint32_t ulNotifiedValue;
// //     ESP_LOGI(TAG, "Start: switch_relays_task");

// //     xTaskNotifyWait(0x00, 0xffffffffUL, &ulNotifiedValue, portMAX_DELAY);

// //     for (;;)
// //     {
// //         xTaskNotifyWait(0x00, 0xffffffffUL, &ulNotifiedValue, portMAX_DELAY);

// //         for (uint8_t i = 0; i < NUMBER_OF_RELAYS; i++)
// //         {
// //             if (relays_status[i])
// //             {
// //                 switch_pin_on(i);
// //             }
// //             else
// //             {
// //                 switch_pin_off(i);
// //             }
// //         }
// //     }
// // }

// // esp_err_t switch_relays_start_task(void)
// // {
// //     xTaskCreate(switch_relays_task, "switch_relays", 1024 * 4, NULL, 3, &SWITCH_RELAYS_TaskHandle);
// //     if (SWITCH_RELAYS_TaskHandle == NULL)
// //     {
// //         ESP_LOGE(TAG, "Fail: SWITCH_RELAYS_TaskHandle");
// //         return ESP_FAIL;
// //     }

// //     return ESP_OK;
// // }

// // void vTimerCheckRelaysState(TimerHandle_t pxTimer)
// // {
// //     bool update_relay = false;

// //     for (uint8_t i = 0; i < 8; i++)
// //     {
// //         if (last_relays_state[i] != relays_status[i])
// //             update_relay = true;
// //     }

// //     for (uint8_t i = 0; i < 8; i++)
// //         last_relays_state[i] = relays_status[i];

// //     if (update_relay)
// //         xTaskNotify(SWITCH_RELAYS_TaskHandle, 0x01, eSetBits);
// // }
