#pragma once

#include <stdint.h>
#include <stdbool.h>


// =============================================
//                  STATE SETTINGS
// =============================================
typedef enum {
    SYS_STATE_INIT = 0,
    SYS_STATE_NORMAL,
    SYS_STATE_ALARM,
    SYS_STATE_MAINTENANCE,
    SYS_STATE_ERROR,
    SYS_STATE_SHUTDOWN
} system_state_t;

typedef struct {
    system_state_t current_state; // Starea activă în acest moment
    bool buzzer_muted;            // Flag pentru a ști dacă s-a apăsat Mute fizic
    bool tank_low_flag;           // Reține dacă plutitorul indică bazin gol
    bool temp_error_flag;         // Reține dacă temperatura apei a încălcat pragurile
} state_manager_t;

// =============================================
//                  WIFI SETTINGS
// =============================================
typedef struct {
    uint8_t wifi_mode;
    char sta_ssid[32];
    char sta_password[64];
    bool sta_enabled;
    char sta_ip[16];
    char hostname[32];
    char ap_ssid[32];          // Numele rețelei emise de ESP32 (NOU)
    char ap_password[64];      // Parola rețelei emise de ESP32 (NOU)
    uint8_t ap_channel;
    bool ap_hidden;
} wifi_settings_t;
// =============================================
//                  TIME SETTINGS
// =============================================
typedef struct {
    char timezone[64];
    bool use_dst;
} time_settings_t;

// =============================================
//                  HYDRO SETTINGS
// =============================================
typedef struct {
    // Timp general
    int start_hour;
    int start_minute;
    int day_period_hours;

    // Setări Pompa Apă (Exemplu: ON=10 min, OFF=20 min pentru 2 cicluri/oră)
    int water_on_min;
    int water_off_min;

    // Setări Pompa Apă - NOAPTE (NOU)
    int water_night_on_min;
    int water_night_off_min;

    // Setări Pompa Aer
    int air_mode;       // 0 = Non-Stop, 1 = Sincron cu apa, 2 = Pre-oxigenare
    int air_pre_min;    // minute de pornire în avans
} hydro_settings_t;

// =============================================
//                  ALARM SETTINGS
// =============================================
typedef struct alarm_settings {
    float water_temp_min;   // Prag minim temperatură apă (ex: 18.0)
    float water_temp_max;   // Prag maxim temperatură apă (ex: 26.5)
    bool float_switch_inv;  // Inversare logică float switch (0=Normal, 1=Inversat)
    bool mute_buzzer;       // Stare Mute pentru alarmă (0=Buzzer activ, 1=Mute)
} alarm_settings_t;

// =============================================
//                  LOG SETTINGS
// =============================================
typedef struct log_settings {
    // Setări noi pentru stocare loguri
    int log_dht_enable;     // 1 = Pornit, 0 = Oprit (Default: 1)
    int log_dht_interval;   // Interval în minute (Default: 5)
    int log_ds_enable;      // 1 = Pornit, 0 = Oprit (Default: 1)
    int log_ds_interval;    // Interval în minute (Default: 5)
} log_settings_t;

// =============================================
//                  GLOBAL CONFIG
// =============================================
typedef struct {
    uint32_t         config_version;
    char             device_name[32];
    
    state_manager_t  state;
    wifi_settings_t  wifi;
    time_settings_t  time;
    hydro_settings_t hydro;
    log_settings_t   logs;
    alarm_settings_t alarm;
    
    // Viitoare module
    // light_settings_t light;
    // air_settings_t   air;
} system_config_t;

// Default values
#define DEFAULT_DEVICE_NAME     "HydroESPro"
#define DEFAULT_TIMEZONE        "EET-2EEST,M3.5.0/3,M10.5.0/4"



extern state_manager_t sys_manager;
extern wifi_settings_t wifi_settings;
extern time_settings_t time_settings;
extern hydro_settings_t hydro_settings;
extern alarm_settings_t alarm_settings;
extern log_settings_t log_settings;