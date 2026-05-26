#include "button.h"
#include "config.h"
#include "state_manager.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DEBOUNCE_DELAY_MS     50     // Timp de filtrare a zgomotului mecanic
#define TICK_DELAY_MS         10     // Frecvența de verificare a pinului (polling)

static const char *TAG = "BUTON_HW";

// Funcția task-ului pentru buton conectată la State Manager
void button_task(void *pvParameters) {
    // 1. Configurare GPIO pentru buton
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,   // Folosește pull-up extern conform config schemei tale
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE      // Polling stabil, fără întreruperi mecano-electrice
    };
    gpio_config(&io_conf);

    int last_state = 1; // Starea anterioară implicită (1 = neapăsat)

    ESP_LOGI(TAG, "Task-ul pentru buton a pornit și este conectat la State Manager.");

    while (1) {
        int current_state = gpio_get_level(BUTTON_GPIO);

        // Detectează tranziția (Front Căzător): de la NEAPĂSAT (1) la APĂSAT (0)
        if (last_state == 1 && current_state == 0) {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY_MS)); // Așteaptă eliminarea zgomotului de contact

            // Verifică din nou dacă butonul este în continuare apăsat cu adevărat
            if (gpio_get_level(BUTTON_GPIO) == 0) {
                ESP_LOGI(TAG, "Buton fizic apăsat legitim! Trimitere eveniment către State Manager.");
                
                // Trimitem comanda direct către creierul central al sistemului
                state_manager_handle_button_press();

                // Sincronizăm și LED-ul de avertizare în funcție de starea de alarmă, dacă este cazul
                // (Opțional: poți folosi state_manager_get_state() pentru a controla LED-ul mai târziu)
            }
        }

        last_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(TICK_DELAY_MS)); // Eliberează CPU pentru celelalte task-uri
    }
}

// Inițializarea task-ului de FreeRTOS
void button_init(void) {
    xTaskCreate(button_task, "button_task", 2048*2, NULL, 1, NULL);
}
