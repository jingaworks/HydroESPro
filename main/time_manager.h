#pragma once

#include "esp_err.h"
#include <time.h>
#include <sys/time.h>
#include "event_bus.h"


typedef struct {
    struct tm timeinfo;
    time_t unix_time;
    bool is_synced;
} time_event_data_t;

esp_err_t time_manager_init(void);

esp_err_t time_manager_sync_with_sntp(void);

struct tm time_manager_get_current_time(void);
time_t    time_manager_get_timestamp(void);

const char* time_manager_get_formatted_time(void);      // HH:MM:SS
const char* time_manager_get_formatted_datetime(void);  // YYYY-MM-DD HH:MM:SS
const char* time_manager_get_formatted_date(void);      // YYYY-MM-DD

esp_err_t time_manager_set_from_browser(struct timeval *tv);

bool time_manager_is_time_valid(void);