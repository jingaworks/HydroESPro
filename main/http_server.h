#pragma once

/**
 * SD CARD
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include <dirent.h>

#include <driver/gpio.h>
#include <lwip/sockets.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_http_server.h>
#include <esp_http_client.h>

#include <esp_transport.h>

#include <time.h>
#include <sys/time.h>


#include "cJSON.h"

#include "keep_alive.h"
#include "wifi_conn.h"


#include "dht_sensor.h"
#include "ds18b20.h"

#include "board.h"
#include "config.h"
#include "sdcard.h"

#include "hydro_manager.h"
#include "storage_manager.h"
#include "time_manager.h"

#include "wifi_conn.h"

#define WEB_MAXDELAY 512
#define WEB_DATA_QUEUE_SIZE 10
#define WEB_DATA_MAX_SIZE 128


typedef struct ws_web_data_t
{
    uint8_t *data;
    int len;
} __attribute__((packed)) ws_web_data_t;

// enum Server_t {SERVER_LOCAL, SERVER_NETWORK};
struct ws_async_resp_arg
{
    httpd_handle_t hd;
    int fd;
    uint8_t *data;
    size_t len;
};

extern TaskHandle_t WEB_SERVER_TaskHandle;
extern TaskHandle_t WEB_STATUS_TaskHandle;

extern httpd_handle_t server;

// extern void ws_send_async(void *arg);

extern esp_err_t start_http_server(void);
extern void stop_server(void);

extern esp_err_t start_server_task(void);
extern esp_err_t start_status_task(void);
