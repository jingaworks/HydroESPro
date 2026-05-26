#include "event_bus.h"
#include <esp_err.h>

static const char *TAG = "EVENT_BUS";

// ==================== DEFINE EVENT BASES ====================
ESP_EVENT_DEFINE_BASE(SYSTEM_EVENTS);
ESP_EVENT_DEFINE_BASE(SENSOR_EVENTS);
ESP_EVENT_DEFINE_BASE(ENVIRONMENT_EVENTS);
ESP_EVENT_DEFINE_BASE(LOG_EVENTS);
ESP_EVENT_DEFINE_BASE(WIFI_EVENTS_EXT);
// ========================================================

static bool event_loop_created = false;

// Default handlers (opțional)
static void default_system_handler(void *arg, esp_event_base_t base, 
                                  int32_t event_id, void *event_data)
{
    if (base == SYSTEM_EVENTS) {
        switch (event_id) {
            case SYS_EVENT_TIME_UPDATED:
                // ESP_LOGV(TAG, "TIME UPDATED");
                break;
            case SYS_EVENT_TIME_SYNCED:
                ESP_LOGI(TAG, "Time synchronized via event bus");
                break;
            default:
                break;
        }
    }
}

esp_err_t event_bus_init(void)
{
    if (!event_loop_created) {
        esp_err_t ret = esp_event_loop_create_default();
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(ret));
            return ret;
        }
        event_loop_created = true;
        ESP_LOGI(TAG, "Default event loop created successfully");
    }

    // Register default handler for system events (debug)
    ESP_ERROR_CHECK(esp_event_handler_register(SYSTEM_EVENTS, 
                    ESP_EVENT_ANY_ID, 
                    default_system_handler, NULL));

    return ESP_OK;
}

esp_err_t event_bus_post(esp_event_base_t base, int32_t event_id, 
                        void *event_data, size_t event_data_size)
{
    return esp_event_post(base, event_id, event_data, 
                         event_data_size, portMAX_DELAY);
}

esp_err_t event_bus_register_handler(esp_event_base_t base, 
                                    int32_t event_id,
                                    esp_event_handler_t handler, 
                                    void *handler_arg)
{
    return esp_event_handler_register(base, event_id, handler, handler_arg);
}

esp_err_t event_bus_unregister_handler(esp_event_base_t base, 
                                      int32_t event_id,
                                      esp_event_handler_t handler)
{
    return esp_event_handler_unregister(base, event_id, handler);
}