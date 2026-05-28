#include "wifi_manager.h"
#include "storage_manager.h"
#include "state_manager.h"
#include "time_manager.h"
#include "event_bus.h"
#include "config.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char* TAG = "WIFI_MGR";

static wifi_system_state_t current_state = WIFI_STATE_APSTA_INIT;
static wifi_status_t status = {0};
static TaskHandle_t wifi_task_handle = NULL;
static bool sta_desired = false;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);

static void wifi_manager_task(void* pvParameters);

// ====================== INIT ======================
esp_err_t wifi_manager_init(void)
{
    esp_netif_init();
    // esp_event_loop_create_default(); // deja făcut în event_bus

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // Încărcăm configurația salvată
    // storage_load_wifi_config(...)  // vom implementa mai târziu

    // Pentru moment pornim implicit în AP+STA pentru setup
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    // Configurare AP implicit
    wifi_config_t ap_config = {
        .ap = {
            .ssid = HYDROESPRO_AP_SSID,
            .ssid_len = strlen(HYDROESPRO_AP_SSID),
            .password = HYDROESPRO_AP_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .channel = 6,
            .ssid_hidden = 0
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    status.ap_active = true;
    current_state = WIFI_STATE_APSTA_ACTIVE;

    // Pornire task dedicat
    xTaskCreate(wifi_manager_task, "wifi_mgr_task", 5120, NULL, 5, &wifi_task_handle);

    ESP_LOGI(TAG, "WiFi Manager started in AP+STA mode (Setup)");
    return ESP_OK;
}

// ====================== EVENT HANDLER ======================
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        sprintf(status.sta_ip, IPSTR, IP2STR(&event->ip_info.ip));

        current_state = WIFI_STATE_STA_CONNECTED;
        status.state = current_state;
        status.sta_retry_count = 0;

        ESP_LOGI(TAG, "STA Connected! IP: %s", status.sta_ip);
        event_bus_post(WIFI_EVENTS, EVENT_WIFI_STA_CONNECTED, NULL, 0);
        event_bus_post(WIFI_EVENTS, EVENT_WIFI_STATE_CHANGED, &status, sizeof(status));

    } else if (event_base == WIFI_EVENT && event_id == EVENT_WIFI_STA_DISCONNECTED) {
        current_state = WIFI_STATE_STA_DISCONNECTED;
        status.state = current_state;
        ESP_LOGW(TAG, "STA Disconnected");
        event_bus_post(WIFI_EVENTS, EVENT_WIFI_STA_DISCONNECTED, NULL, 0);
        event_bus_post(WIFI_EVENTS, EVENT_WIFI_STATE_CHANGED, &status, sizeof(status));
    }
}

// ====================== TASK DEDICAT ======================
static void wifi_manager_task(void* pvParameters)
{
    while (1) {
        switch (current_state) {
            case WIFI_STATE_STA_CONNECTED:
                // Dacă e stabil, putem opri AP-ul (dacă userul a ales STA Only)
                break;

            case WIFI_STATE_STA_DISCONNECTED:
                if (sta_desired && status.sta_retry_count < MAX_STA_RETRY) {
                    status.sta_retry_count++;
                    ESP_LOGI(TAG, "Trying to reconnect STA... (%d)", status.sta_retry_count);
                    esp_wifi_connect();
                }
                break;

            default:
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// ====================== API ======================
esp_err_t wifi_manager_set_mode(bool sta_enabled)
{
    sta_desired = sta_enabled;
    // vom implementa logica completă mai târziu
    event_bus_post(WIFI_EVENTS, EVENT_WIFI_STATE_CHANGED, &status, sizeof(status));
    return ESP_OK;
}

esp_err_t wifi_manager_connect_sta(const char* ssid, const char* password)
{
    if (!ssid) return ESP_ERR_INVALID_ARG;

    wifi_config_t sta_config = {0};
    strncpy((char*)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
    if (password) {
        strncpy((char*)sta_config.sta.password, password, sizeof(sta_config.sta.password));
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    current_state = WIFI_STATE_STA_CONNECTING;
    status.sta_retry_count = 0;

    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);
    return ESP_OK;
}

wifi_system_state_t wifi_manager_get_state(void)
{
    return current_state;
}

const char* wifi_manager_get_ip(void)
{
    return (status.sta_ip[0] != '\0') ? status.sta_ip : "192.168.4.1";
}


// #include "wifi_manager.h"
// #include "storage_manager.h"
// #include "state_manager.h"

// #include <esp_wifi.h>
// #include <esp_netif.h>
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>
// #include <string.h>

// static const char* TAG = "WIFI_MGR";

// static wifi_system_state_t current_state = WIFI_STATE_APSTA_INIT;
// static wifi_status_t status = {0};
// static TaskHandle_t wifi_task_handle = NULL;

// static void wifi_event_handler(void* arg, esp_event_base_t event_base,
//                                int32_t event_id, void* event_data);

// static void wifi_manager_task(void* pvParameters);

// // ====================== INIT ======================
// esp_err_t wifi_manager_init(void)
// {
//     esp_netif_init();
//     // am creat-o in event_bus_init
//     // esp_event_loop_create_default();

//     // trebuie sa verificam in wifi_settings daca exista date de configurare pentru wifi mode sa vedem ce creaza.
//     esp_netif_create_default_wifi_ap();
//     esp_netif_create_default_wifi_sta();

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     // Start in AP+STA for setup
//     // trebuie sa verificam in wifi_settings daca exista date de configurare pentru wifi mode sa vedem ce pornim.
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

//     ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
//     ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

//     // Configurare AP inițială?
//     // trebuie sa verificam in wifi_settings daca exista date de configurare pentru AP si STA.
//     wifi_config_t ap_config = {
//         .ap = {
//             .ssid = HYDROESPRO_AP_SSID,
//             .ssid_len = strlen(HYDROESPRO_AP_SSID),
//             .password = HYDROESPRO_AP_PASS,
//             .max_connection = 5,
//             .authmode = WIFI_AUTH_WPA2_PSK,
//             .channel = 6,
//             .ssid_hidden = 0
//         }
//     };
//     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
//     ESP_ERROR_CHECK(esp_wifi_start());

//     // depinde de ce setari exista
//     status.ap_active = true;
//     status.sta_desired = false;
//     current_state = WIFI_STATE_APSTA_ACTIVE;

//     xTaskCreate(wifi_manager_task, "wifi_mgr_task", 5120, NULL, 6, &wifi_task_handle);

//     ESP_LOGI(TAG, "HydroESPro WiFi Manager started - AP+STA mode (Setup)");
//     return ESP_OK;
// }

// // ====================== EVENT HANDLER ======================
// static void wifi_event_handler(void* arg, esp_event_base_t event_base,
//                                int32_t event_id, void* event_data)
// {
//     if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
//         ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
//         sprintf(status.sta_ip, IPSTR, IP2STR(&event->ip_info.ip));

//         current_state = WIFI_STATE_STA_CONNECTED;
//         status.state = current_state;

//         event_bus_post(WIFI_EVENT, EVENT_WIFI_STA_CONNECTED, NULL, 0);
//         event_bus_post(WIFI_EVENT, EVENT_WIFI_STATE_CHANGED, &status, sizeof(status));

//         ESP_LOGI(TAG, "STA Connected! IP: %s", status.sta_ip);
//     }
//     else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
//         current_state = WIFI_STATE_STA_DISCONNECTED;
//         status.state = current_state;
//         event_bus_post(WIFI_EVENT, EVENT_WIFI_STA_DISCONNECTED, NULL, 0);
//         event_bus_post(WIFI_EVENT, EVENT_WIFI_STATE_CHANGED, &status, sizeof(status));
//     }
// }

// // ====================== TASK DEDICAT ======================
// static void wifi_manager_task(void* pvParameters)
// {
//     while (1) {
//         switch (current_state) {
//             case WIFI_STATE_STA_CONNECTED:
//                 // Așteptăm să fie stabil, apoi trecem în mod curat STA_ONLY
//                 vTaskDelay(pdMS_TO_TICKS(STA_STABLE_DELAY_MS));
//                 if (current_state == WIFI_STATE_STA_CONNECTED) {
//                     ESP_LOGI(TAG, "STA stable → Switching to WIFI_MODE_STA (AP off)");
//                     esp_wifi_set_mode(WIFI_MODE_STA);
//                     status.ap_active = false;
//                     current_state = WIFI_STATE_STA_ONLY;
//                     event_bus_post(WIFI_EVENT, EVENT_WIFI_AP_STOPPED, NULL, 0);
//                     event_bus_post(WIFI_EVENT, EVENT_WIFI_STATE_CHANGED, &status, sizeof(status));
//                 }
//                 break;

//             case WIFI_STATE_STA_DISCONNECTED:
//                 if (status.sta_desired) {
//                     status.sta_retry_count++;
//                     if (status.sta_retry_count >= MAX_STA_RETRY) {
//                         // Fallback la AP+STA
//                         esp_wifi_set_mode(WIFI_MODE_APSTA);
//                         current_state = WIFI_STATE_FALLBACK_APSTA;
//                         status.ap_active = true;
//                         event_bus_post(WIFI_EVENT, EVENT_WIFI_FALLBACK_STARTED, NULL, 0);
//                     }
//                 }
//                 break;

//             default:
//                 break;
//         }
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }

// // la ce o folosim?
// // ====================== API ======================
// esp_err_t wifi_manager_set_mode(bool sta_desired)
// {
//     status.sta_desired = sta_desired;

//     if (!sta_desired) {
//         // AP Only forced
//         esp_wifi_set_mode(WIFI_MODE_AP);
//         current_state = WIFI_STATE_APSTA_ACTIVE;
//         status.ap_active = true;
//     }

//     event_bus_post(WIFI_EVENT, EVENT_WIFI_STATE_CHANGED, &status, sizeof(status));
//     return ESP_OK;
// }

// // la ce o folosim?
// esp_err_t wifi_manager_connect_sta(const char* ssid, const char* password)
// {
//     wifi_config_t sta_config = {0};
//     strncpy((char*)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
//     strncpy((char*)sta_config.sta.password, password, sizeof(sta_config.sta.password));

//     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
//     ESP_ERROR_CHECK(esp_wifi_connect());

//     current_state = WIFI_STATE_STA_CONNECTING;
//     status.sta_retry_count = 0;

//     event_bus_post(WIFI_EVENT, EVENT_WIFI_STATE_CHANGED, &status, sizeof(status));
//     return ESP_OK;
// }

// // cand o folosim si cum/cand luam ip-ul?
// const char* wifi_manager_get_ip(void)
// {
//     return (status.sta_ip[0] != '\0') ? status.sta_ip : "192.168.4.1";
// }

// wifi_system_state_t wifi_manager_get_state(void)
// {
//     return current_state;
// }