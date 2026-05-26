#pragma once

#include <inttypes.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ds18x20.h>
#include <esp_log.h>
#include <esp_err.h>

#include "storage_manager.h"
#include "utill.h"



#define SENSOR_DS_GPIO      4
#define MAX_DS_SENSORS      1
#define DS_SENSOR_DELAY_MS  5000

#define DS_DATA_LOG_INTERVAL (300 / (DS_SENSOR_DELAY_MS / 1000))


extern float ds_temp;

extern bool ds_valid_reding;

// ds sensor read data temp
typedef struct ds_data_t {
    uint8_t id;
    onewire_addr_t address;
    float tempC;
    float tempF;
    uint32_t read_time;
} ds_data_t;

// ds available sensors for read data temp 
typedef struct ds_sensors_t {
    uint8_t sensors_num;
    ds_data_t data[MAX_DS_SENSORS];
} ds_sensors_t;

// ds available sensors data temp
extern ds_sensors_t ds_sensors;

extern esp_err_t start_ds18b20_task(void);