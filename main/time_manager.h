#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <time.h>
// #include "event_bus.h"


extern char current_timezone[64] ;

// ==================== PUBLIC API ====================
esp_err_t time_manager_init(void);

esp_err_t time_manager_set_timezone(const char *tz);
const char* time_manager_get_timezone(void);

// esp_err_t time_manager_sync_from_sntp(void);
esp_err_t time_manager_set_time(struct tm *timeinfo);

time_t time_manager_get_unix_time(void);
struct tm time_manager_get_tm(void);           // returns copy of rtc_time
bool time_manager_is_synced(void);

void time_manager_print_current_time(void);

// ==================== Legacy / Compatibility ====================
// Temporar - vor fi eliminate treptat
extern struct tm rtc_time;
extern bool time_synchronized;

// ==================== TAG ====================
extern const char *TIME_MANAGER_TAG;
