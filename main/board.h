#pragma once

#include <stdint.h>
#include <esp_err.h>
#include <esp_log.h>
#include <nvs.h>


// Enumerarea tuturor stărilor posibile ale sistemului
typedef enum {
    SYS_STATE_INIT = 0,
    SYS_STATE_NORMAL,
    SYS_STATE_MAINTENANCE,  // Modul Schimb Apă
    SYS_STATE_ALARM,         // Modul Alarmă Activă,
    SYS_STATE_ERROR
} system_state_t;

// Structura centrală de stare a managerului
typedef struct {
    system_state_t current_state; // Starea activă în acest moment
    bool buzzer_muted;            // Flag pentru a ști dacă s-a apăsat Mute fizic
    bool tank_low_flag;           // Reține dacă plutitorul indică bazin gol
    bool temp_error_flag;         // Reține dacă temperatura apei a încălcat pragurile
} state_manager_t;


// Structură pentru evenimente
typedef struct {
    system_state_t old_state;
    system_state_t new_state;
    const char* reason;
} state_change_event_t;

typedef struct wifi_settings {
    int wifi_mode;             // 0 = AP (Implicit Factory), 1 = STA
    char sta_ssid[32];         // Numele routerului salvat permanent
    char sta_password[64];     // Parola routerului salvată permanent
    char ap_ssid[32];          // Numele rețelei emise de ESP32 (NOU)
    char ap_password[64];      // Parola rețelei emise de ESP32 (NOU)
} wifi_settings_t;

// Structură pentru setările venite din pagina Web
typedef struct hydro_settings {
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

typedef struct alarm_settings {
    float water_temp_min;   // Prag minim temperatură apă (ex: 18.0)
    float water_temp_max;   // Prag maxim temperatură apă (ex: 26.5)
    bool float_switch_inv;  // Inversare logică float switch (0=Normal, 1=Inversat)
    bool mute_buzzer;       // Stare Mute pentru alarmă (0=Buzzer activ, 1=Mute)
} alarm_settings_t;

typedef struct log_settings {
    // Setări noi pentru stocare loguri
    int log_dht_enable;     // 1 = Pornit, 0 = Oprit (Default: 1)
    int log_dht_interval;   // Interval în minute (Default: 5)
    int log_ds_enable;      // 1 = Pornit, 0 = Oprit (Default: 1)
    int log_ds_interval;    // Interval în minute (Default: 5)
} log_settings_t;

/***********************************************/
/*** Board preferences *************************/

// Declarația instanței globale a managerului de stare
extern state_manager_t sys_manager;

extern hydro_settings_t hydro_settings;
extern alarm_settings_t alarm_settings;
extern log_settings_t log_settings;


extern wifi_settings_t wifi_settings;

// Adaugă și prototipurile de salvare NVS specifice în board.h:
extern esp_err_t erase_wifi_config(void);
extern esp_err_t save_wifi_config(void);
extern esp_err_t load_wifi_config(void);



extern esp_err_t erase_board_config(void);
extern esp_err_t save_board_config(void);
extern esp_err_t load_board_config(void);

extern esp_err_t erase_hydro_config(void);
extern esp_err_t save_hydro_config(void);
extern esp_err_t load_hydro_config(void);

extern esp_err_t erase_alarm_config(void);
extern esp_err_t save_alarm_config(void);
extern esp_err_t load_alarm_config(void);

extern esp_err_t erase_log_config(void);
extern esp_err_t save_log_config(void);
extern esp_err_t load_log_config(void);

extern void load_all_config(void);















// de sters


typedef struct env_devices {
    bool light_alarm;
    bool light;
    bool humidifier;
    bool dehumidifier;
    bool heater;
    bool cooler;
} env_devices_t;

typedef struct env_stealth {
    bool use;
    uint16_t duration;
    uint8_t start_h;
    uint8_t start_m;
} env_stealth_t;

typedef struct env_schedule {
    uint8_t type;
    uint16_t grow_duration;
    uint8_t grow_start_h;
    uint8_t grow_start_m;
    uint16_t bloom_duration;
    uint8_t bloom_start_h;
    uint8_t bloom_start_m;
    uint16_t begin_y;
    uint8_t begin_m;
    uint8_t begin_d;
    bool seedling_use;
    uint8_t seedling_days;
    uint8_t temp;
} env_schedule_t;

typedef struct env_humidity {
    uint8_t week[20];
} env_humidity_t;

typedef struct env_settings {
    env_devices_t devices;
    env_stealth_t stealth;
    env_schedule_t schedule;
    env_humidity_t humidity;
} env_settings_t;


extern env_settings_t env_settings;