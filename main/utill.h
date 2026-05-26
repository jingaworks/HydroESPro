#pragma once

#include <stdio.h>
#include <string.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <sys/time.h>

#include "board.h"


typedef struct Web_data
{
    uint8_t type;
    char var[64];
    union
    {
        int val;
        char str[64];
    } data;
} Web_data;

enum WEB_Updates
{
    NO_UPDATES,
    NEW_ENVIRONMENT,
    NEW_HUMIDITIES,
    NEW_WEBSOCKET
};

extern void upTime(char *ret);

extern uint32_t get_time_sec(void);

extern size_t calloc_data(char **p, const char *msg, size_t size);
