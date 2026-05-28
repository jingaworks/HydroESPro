#include "pcf_manager.h"
#include "pins.h"
#include "i2c_manager.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "PCF_MGR";

static uint8_t pcf_output_state = 0xFF;      // Toate OFF
static uint8_t last_input_state = 0xFF;
static TaskHandle_t pcf_task_handle = NULL;

static void pcf_input_task(void* pvParameters);

static void IRAM_ATTR pcf_isr_handler(void* arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(pcf_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// ====================== INIT ======================
esp_err_t pcf_manager_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "PCF Manager initializing...");

    // ====================== NPN ENABLE CONTROL ======================
    gpio_set_direction(PCF_ENABLE_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(PCF_ENABLE_PIN, 0);        // PCF dezactivat la pornire
    vTaskDelay(pdMS_TO_TICKS(30));

    // Pornim alimentarea după perioada critică de boot
    gpio_set_level(PCF_ENABLE_PIN, 1);
    ESP_LOGI(TAG, "PCF8574 modules powered ON via NPN transistor");
    
    vTaskDelay(pdMS_TO_TICKS(60));            // Timp suficient de stabilizare

    // ====================== FORȚĂM TOATE OUTPUT-URILE OFF ======================
    pcf_output_state = 0xFF;                  // Toate releele OFF (Active HIGH logic pe PCF)
    
    ret = pcf_write(PCF8574_OUTPUT_ADDR, pcf_output_state);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "All relays and outputs forced OFF after power enable");
    } else {
        ESP_LOGE(TAG, "Failed to force outputs OFF!");
    }

    // ====================== Interrupt Setup ======================
    gpio_set_direction(PCF_INTERRUPT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PCF_INTERRUPT_PIN, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(PCF_INTERRUPT_PIN, GPIO_INTR_NEGEDGE);

    ret = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "ISR service install failed");
    } else {
        gpio_isr_handler_add(PCF_INTERRUPT_PIN, pcf_isr_handler, NULL);
    }

    // Citire stare inițială butoane + float switch
    pcf_read(PCF8574_INPUT_ADDR, &last_input_state);

    // Task pentru debounce și procesare input
    xTaskCreate(pcf_input_task, "pcf_input_task", 4096, NULL, 5, &pcf_task_handle);

    ESP_LOGI(TAG, "PCF Manager initialized successfully (with power control)");
    return ESP_OK;
}
// ====================== ISR + TASK ======================

static void pcf_input_task(void* pvParameters)
{
    uint8_t current;
    uint32_t last_change = 0;

    while (1) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));

        if (pcf_read(PCF8574_INPUT_ADDR, &current) == ESP_OK) {
            if (current != last_input_state) {
                uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if (now - last_change > 50) {        // Debounce
                    last_input_state = current;
                    last_change = now;

                    event_bus_post(SYSTEM_EVENTS, EVENT_PCF_INPUT_CHANGED, &current, sizeof(current));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ====================== LOW LEVEL ======================
esp_err_t pcf_write(uint8_t pcf_addr, uint8_t value)
{
    return i2c_manager_write_byte(pcf_addr, value);
}

esp_err_t pcf_read(uint8_t pcf_addr, uint8_t *value)
{
    return i2c_manager_read_byte(pcf_addr, value);
}

esp_err_t pcf_set_output(uint8_t pin, bool state)
{
    if (pin > 7) return ESP_ERR_INVALID_ARG;

    if (state)
        pcf_output_state &= ~(1 << pin);     // Activează (Active LOW)
    else
        pcf_output_state |= (1 << pin);      // Dezactivează

    esp_err_t ret = pcf_write(PCF8574_OUTPUT_ADDR, pcf_output_state);
    
    if (ret == ESP_OK) {
        event_bus_post(SYSTEM_EVENTS, EVENT_PCF_OUTPUT_CHANGED, NULL, 0);
    }
    return ret;
}


bool pcf_get_input(uint8_t pin)
{
    if (pin > 7) return false;
    return (last_input_state & (1 << pin)) == 0;   // Active LOW
}