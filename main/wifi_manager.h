#pragma once

#include "esp_err.h"
#include "event_bus.h"

#define HYDROESPRO_AP_SSID          "HydroESPro-Setup"
#define HYDROESPRO_AP_PASS          "hydroesp123"
#define MAX_STA_RETRY               5
#define STA_STABLE_DELAY_MS         10000   // 10 secunde înainte de a opri AP-ul

typedef enum {
    WIFI_STATE_APSTA_INIT,
    WIFI_STATE_APSTA_ACTIVE,        // Setup phase - AP vizibil
    WIFI_STATE_STA_CONNECTING,
    WIFI_STATE_STA_CONNECTED,       // Vom trece în WIFI_MODE_STA
    WIFI_STATE_STA_DISCONNECTED,
    WIFI_STATE_STA_ONLY,            // Mod curat - doar STA
    WIFI_STATE_FALLBACK_APSTA,      // STA a picat → revenim cu AP
    WIFI_STATE_ERROR
} wifi_system_state_t;

typedef struct {
    wifi_system_state_t state;
    char sta_ssid[32];
    char sta_ip[16];
    bool ap_active;
    int sta_retry_count;
    bool sta_desired;               // User wants STA mode
} wifi_status_t;

// mai bine folosim din astea, le combinam pe un singur struct
// typedef struct wifi_settings {
//     int wifi_mode;             // 0 = AP (Implicit Factory), 1 = STA
//     char sta_ssid[32];         // Numele routerului salvat permanent
//     char sta_password[64];     // Parola routerului salvată permanent
//     char ap_ssid[32];          // Numele rețelei emise de ESP32 (NOU)
//     char ap_password[64];      // Parola rețelei emise de ESP32 (NOU)
// } wifi_settings_t;

// Evenimente - le-am mutat in event_bus.
// typedef enum {
//     EVENT_WIFI_STATE_CHANGED = 0x200,
//     EVENT_WIFI_STA_CONNECTED,
//     EVENT_WIFI_STA_DISCONNECTED,
//     EVENT_WIFI_STA_FAILED,
//     EVENT_WIFI_AP_STOPPED,
//     EVENT_WIFI_FALLBACK_STARTED
// } wifi_event_id_t;

// API Public
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_set_mode(bool sta_desired);
esp_err_t wifi_manager_start_scan(void);
esp_err_t wifi_manager_connect_sta(const char* ssid, const char* password);
// trebuie sa facem asta pentru a putea fi apelat?
esp_err_t wifi_manager_disconnect_sta(void);

wifi_system_state_t wifi_manager_get_state(void);
const wifi_status_t* wifi_manager_get_status(void);
const char* wifi_manager_get_ip(void);

// trebuie sa facem asta pentru a putea fi apelat
void wifi_manager_register_event_handler(esp_event_handler_t handler);