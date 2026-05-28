
#include "buzzer.h"
#include "board.h"
#include "config.h"
#include "event_bus.h"
#include "state_manager.h"
#include "time_manager.h"

#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>

static const char *TAG = "BUZZER_QUEUE";

// Coada de priorități
static buzzer_alarm_t alarm_queue[MAX_ALARMS_IN_QUEUE];
static int queue_size = 0;

// Variabile de control pentru ciclul curent
static uint32_t current_cycle_count = 0;
static bool alarm_interrupted = false;
static TaskHandle_t buzzer_task_handle = NULL;
static portMUX_TYPE queue_mux = portMUX_INITIALIZER_UNLOCKED; // Protecție thread-safe pentru coadă

// =============================================
// EVENT HANDLER
// =============================================
static void buzzer_system_event_handler(void *arg, esp_event_base_t base,
                                       int32_t event_id, void *event_data)
{
    if (base != SYSTEM_EVENTS) return;

    switch (event_id) {
        case EVENT_ALARM_TRIGGERED:
            if (!sys_manager.buzzer_muted) {
                ESP_LOGI(TAG, "Event: ALARM_TRIGGERED → Starting alarm pattern");
                // Poți porni o alarmă generică sau specifica
                buzzer_trigger_alarm((buzzer_alarm_t){
                    .alarm_id = ID_APA_CALDA,
                    .on_ms = 150, .off_ms = 400, .beep_count = 4,
                    .pause_ms = 1500, .total_cycles = 0, .priority = 1
                });
            }
            break;

        case EVENT_SYSTEM_MAINTENANCE_START:
            ESP_LOGI(TAG, "Event: MAINTENANCE_START → Starting maintenance beep");
            buzzer_trigger_alarm((buzzer_alarm_t){
                .alarm_id = 99, // ID special pentru mentenanta
                .on_ms = 100, .off_ms = 3000, .beep_count = 1,
                .pause_ms = 0, .total_cycles = 0, .priority = 3
            });
            break;

        case EVENT_SYSTEM_STATE_CHANGED: {
            state_change_event_t *ev = (state_change_event_t *)event_data;
            
            if (ev->new_state == SYS_STATE_NORMAL) {
                ESP_LOGI(TAG, "Event: STATE_CHANGED to NORMAL → Stopping all alarms");
                buzzer_stop_all_alarms();
            }
            else if (ev->new_state == SYS_STATE_MAINTENANCE) {
                // Deja gestionat mai sus prin MAINTENANCE_START
            }
            break;
        }

        case EVENT_SYSTEM_MAINTENANCE_END:
            buzzer_stop_all_alarms();
            break;
    }
}

// Helper: Spargem delay-urile mari în pași de 10ms. Dacă apare o alarmă mai importantă, ieșim instant.
static bool interruptible_delay(uint32_t ms) {
    uint32_t steps = ms / 10;
    for (uint32_t i = 0; i < steps; i++) {
        if (alarm_interrupted) return true;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (ms % 10 > 0 && !alarm_interrupted) {
        vTaskDelay(pdMS_TO_TICKS(ms % 10));
    }
    return alarm_interrupted;
}


// Task-ul care rulează alarma din vârful cozii (indexul 0)
static void buzzer_queue_task(void *pvParameters) {
    while (1) {
        portENTER_CRITICAL(&queue_mux);
        int current_size = queue_size;
        portEXIT_CRITICAL(&queue_mux);

        if (current_size == 0) {
            gpio_set_level(BUZZER_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // Luăm alarma cu prioritatea cea mai mare (întotdeauna la indexul 0)
        buzzer_alarm_t active_alarm = alarm_queue[0];
        alarm_interrupted = false;

        ESP_LOGD(TAG, "Ruleaza alarma ID: %d, Ciclul: %d", active_alarm.alarm_id, current_cycle_count + 1);

        // Executăm seria de bipurii dintr-un ciclu
        for (uint32_t i = 0; i < active_alarm.beep_count; i++) {
            gpio_set_level(BUZZER_GPIO, 1);
            if (interruptible_delay(active_alarm.on_ms)) break;

            gpio_set_level(BUZZER_GPIO, 0);
            if (i < active_alarm.beep_count - 1) {
                if (interruptible_delay(active_alarm.off_ms)) break;
            }
        }

        // Pauza lungă dintre cicluri
        if (!alarm_interrupted) {
            gpio_set_level(BUZZER_GPIO, 0);
            interruptible_delay(active_alarm.pause_ms);
        }

        // Dacă ciclul s-a terminat complet fără întreruperi de prioritate
        if (!alarm_interrupted) {
            current_cycle_count++;

            // Dacă alarma a atins numărul maxim de cicluri setat (și nu este infinită, adică 0)
            if (active_alarm.total_cycles > 0 && current_cycle_count >= active_alarm.total_cycles) {
                ESP_LOGI(TAG, "Alarma ID %d s-a incheiat natural. O eliminam din coada.", active_alarm.alarm_id);
                buzzer_clear_alarm_by_id(active_alarm.alarm_id);
            }
        }
    }
}

void buzzer_init(void) {
    gpio_config_t bzr_conf = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&bzr_conf);
    gpio_set_level(BUZZER_GPIO, 0);

    portENTER_CRITICAL(&queue_mux);
    queue_size = 0;
    current_cycle_count = 0;
    portEXIT_CRITICAL(&queue_mux);

    if (buzzer_task_handle == NULL) {
        xTaskCreate(buzzer_queue_task, "buzzer_queue_task", 3072, NULL, 6, &buzzer_task_handle);
    }

    // ==================== ABONARE LA EVENT BUS ====================
    event_bus_register_handler(SYSTEM_EVENTS, EVENT_ALARM_TRIGGERED,
        buzzer_system_event_handler, NULL);

    event_bus_register_handler(SYSTEM_EVENTS, EVENT_SYSTEM_MAINTENANCE_START,
        buzzer_system_event_handler, NULL);

    event_bus_register_handler(SYSTEM_EVENTS, EVENT_SYSTEM_STATE_CHANGED,
        buzzer_system_event_handler, NULL);

    ESP_LOGI(TAG, "Buzzer initialized with Event Bus support");
}

void buzzer_trigger_alarm(buzzer_alarm_t alarm) {
    portENTER_CRITICAL(&queue_mux);

    // 1. Verificăm dacă această alarmă este deja în coadă (să nu o duplicăm)
    for (int i = 0; i < queue_size; i++) {
        if (alarm_queue[i].alarm_id == alarm.alarm_id) {
            portEXIT_CRITICAL(&queue_mux);
            return;
        }
    }

    // 2. Verificăm dacă avem loc în coadă
    if (queue_size >= MAX_ALARMS_IN_QUEUE) {
        // ESP_LOGE(TAG, "Coada de alarme este plina! Ignoram ID: %d", alarm.alarm_id);
        portEXIT_CRITICAL(&queue_mux);
        return;
    }

    // 3. Găsim poziția corectă de inserare bazată pe prioritate (Sortare la inserție)
    int insert_index = queue_size;
    while (insert_index > 0 && alarm_queue[insert_index - 1].priority > alarm.priority) {
        alarm_queue[insert_index] = alarm_queue[insert_index - 1]; // Mutăm elementele mai puțin importante la dreapta
        insert_index--;
    }

    alarm_queue[insert_index] = alarm;
    queue_size++;

    // 4. Dacă noua alarmă a ajuns pe poziția 0, înseamnă că este mai importantă decât ce suna acum!
    if (insert_index == 0) {
        // ESP_LOGI(TAG, "Alarma ID %d suspenda alarma veche! Are prioritate mai mare (%d).", alarm.alarm_id, alarm.priority);
        alarm_interrupted = true; // Semnalizăm task-ului să oprească delay-urile imediat
        current_cycle_count = 0;  // Resetăm contorul de cicluri pentru noua alarmă activă
    }
    else {
     // ESP_LOGI(TAG, "Alarma ID %d adaugata in fundal (Pozitia %d in coada).", alarm.alarm_id, insert_index);
    }

    portEXIT_CRITICAL(&queue_mux);
}

void buzzer_clear_alarm_by_id(uint8_t alarm_id) {
    portENTER_CRITICAL(&queue_mux);

    int found_index = -1;
    for (int i = 0; i < queue_size; i++) {
        if (alarm_queue[i].alarm_id == alarm_id) {
            found_index = i;
            break;
        }
    }

    if (found_index == -1) {
        portEXIT_CRITICAL(&queue_mux);
        return; // Nu a fost găsită
    }

    // Mutăm restul alarmelor înapoi pentru a acoperi golul
    for (int i = found_index; i < queue_size - 1; i++) {
        alarm_queue[i] = alarm_queue[i + 1];
    }
    queue_size--;

    // Dacă am șters chiar alarma care suna în acel moment (indexul 0)
    if (found_index == 0) {
        // ESP_LOGI(TAG, "Alarma activa ID %d a fost stearsa. Se trece la urmatoarea din coada.", alarm_id);
        alarm_interrupted = true; // Oprim delay-urile curente
        current_cycle_count = 0;  // Resetăm contorul pentru ce urmează din coadă
        gpio_set_level(BUZZER_GPIO, 0);
    }

    portEXIT_CRITICAL(&queue_mux);
}

void buzzer_stop_all_alarms(void) {
    portENTER_CRITICAL(&queue_mux);
    queue_size = 0;
    current_cycle_count = 0;
    alarm_interrupted = true;
    gpio_set_level(BUZZER_GPIO, 0);
    // ESP_LOGI(TAG, "Toate alarmele au fost resetate si sterse.");
    portEXIT_CRITICAL(&queue_mux);
}

void buzzer_beep_direct(uint32_t duration_ms) {
    // Beep hardware rapid care nu modifică stările cozii
    gpio_set_level(BUZZER_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    gpio_set_level(BUZZER_GPIO, 0);
}
/**
 * Returneaza true daca buzzerul ruleaza o alarma
*/
bool buzzer_is_beeping(void) {
    bool status = false;

    // Protejăm citirea variabilei globale împotriva modificărilor simultane
    portENTER_CRITICAL(&queue_mux);
    if (queue_size > 0) {
        status = true;
    }
    portEXIT_CRITICAL(&queue_mux);

    return status;
}

bool buzzer_is_alarm_active(uint8_t alarm_id) {
    bool found = false;

    portENTER_CRITICAL(&queue_mux);
    // Căutăm prin toate alarmele prezente în coadă
    for (int i = 0; i < queue_size; i++) {
        if (alarm_queue[i].alarm_id == alarm_id) {
            found = true;
            break; // Am găsit-o, putem opri căutarea
        }
    }
    portEXIT_CRITICAL(&queue_mux);

    return found;
}
