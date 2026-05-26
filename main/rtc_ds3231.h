#pragma once

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include <time.h>
#include <sys/time.h>

#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "nvs_flash.h"
#include "esp_sntp.h"

#include <ds3231.h>

#include "time_manager.h"
#include "config.h"


extern esp_err_t init_rtc_ds3231(void);

extern esp_err_t rtc_ds3231_set_system_time(void);

extern esp_err_t rtc_ds3231_set_module_time(struct tm timeinfo);

extern esp_err_t set_ds_browser_time(struct timeval *tv);

// Legacy (temporary)
extern struct tm rtc_time;
extern bool time_synchronized;

extern const char *RTC_DS3231_TAG;