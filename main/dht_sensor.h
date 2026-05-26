#pragma once

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/timers.h"
#include "config.h"
#include <dht.h>

#include "sensor_manager.h"

#define DHT_SENSOR_TYPE             DHT_TYPE_AM2301
#define DHT_SENSOR_DELAY_MS         10000

#define DHT_DATA_LOG_INTERVAL       (300 / (DHT_SENSOR_DELAY_MS / 1000))

#define DHT_MAX_CONSECUTIVE_ERRORS  5

extern float dht_temp;
extern float dht_humid;
extern bool dht_valid_reading;

extern esp_err_t start_dht_task(void);