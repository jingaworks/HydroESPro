#pragma once

#include "esp_err.h"
#include <time.h>
#include <sys/time.h>

#include <stdbool.h>

extern char current_timezone[64];     // timezone global, accesibil și din time_manager

esp_err_t rtc_manager_init(void);

esp_err_t rtc_manager_read_time(struct tm *timeinfo);     // Citește raw din DS3231
esp_err_t rtc_manager_write_time(struct tm *timeinfo);

esp_err_t rtc_manager_set_system_time(void);              // Citește + aplică TZ + DST = -1

bool rtc_manager_is_available(void);

const char* rtc_manager_get_formatted_time(void);         // Formatare completă