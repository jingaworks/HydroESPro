#include "event_bus.h"
#include <esp_log.h>

static const char *TAG = "EVENT_BUS";

static bool event_loop_created = false;

// ==================== DEFINE ALL BASES ====================
ESP_EVENT_DEFINE_BASE(SYSTEM_EVENTS);
ESP_EVENT_DEFINE_BASE(WIFI_EVENTS);
ESP_EVENT_DEFINE_BASE(PCF_EVENTS);
ESP_EVENT_DEFINE_BASE(HYDRO_EVENTS);
ESP_EVENT_DEFINE_BASE(LOG_EVENTS);
ESP_EVENT_DEFINE_BASE(SENSOR_EVENTS);

// ========================================================

static void default_system_handler(void *arg, esp_event_base_t base, 
                                  int32_t event_id, void *event_data)
{
    if (base == SYSTEM_EVENTS) {
        switch (event_id) {
            case EVENT_TIME_SNTP_SYNC_SUCCESS:
                ESP_LOGI(TAG, "Time synchronized via Event Bus successfully");
                break;
            case EVENT_TIME_SNTP_SYNC_FAILED:
                ESP_LOGI(TAG, "Time synchronized via Event Bus failed");
                break;
            case EVENT_SYSTEM_REBOOT_REQUESTED:
                ESP_LOGW(TAG, "Reboot requested!");
                break;
            default:
                break;
        }
    }
};

esp_err_t event_bus_init(void)
{
    if (event_loop_created) return ESP_OK;

    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    event_loop_created = true;

    // Register default handler
    esp_event_handler_register(SYSTEM_EVENTS, ESP_EVENT_ANY_ID, 
                              default_system_handler, NULL);

    ESP_LOGI(TAG, "Event Bus initialized successfully");
    return ESP_OK;
}

esp_err_t event_bus_post(esp_event_base_t base, int32_t event_id, 
                        void *event_data, size_t event_data_size)
{
    return esp_event_post(base, event_id, event_data, event_data_size, pdMS_TO_TICKS(100));
}

esp_err_t event_bus_register_handler(esp_event_base_t base, int32_t event_id,
                                    esp_event_handler_t handler, void *handler_arg)
{
    return esp_event_handler_register(base, event_id, handler, handler_arg);
}
