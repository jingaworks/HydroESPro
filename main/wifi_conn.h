#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <driver/gpio.h>
#include <esp_wifi.h>
#include <esp_http_server.h>

#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/dns.h"

#include "http_server.h"
#include "keep_alive.h"

#include "utill.h"
#include "board.h"

#define ESP_WIFI_AP_SSID "ESP_MAIN"
#define ESP_WIFI_AP_PASS "123456789"

/**
 *
 */
extern TaskHandle_t RECONNECT_WIFI_TaskHandle;
#define CONFIG_ESPNOW_ENABLE_LONG_RANGE

#if !CONFIG_HTTPD_WS_SUPPORT
#error This example cannot be used unless HTTPD_WS_SUPPORT is enabled in esp-http-server component configuration
#endif

extern char user_wifi_cc[45][3];
extern uint8_t user_wifi_cc_index;

extern char ip_address[16];
/**
 * Indicate if wifi is connected
 */
extern bool wifi_connected;

/***********************************************/
/*** WiFi Status *******************************/
/**
 *
 */
extern esp_err_t start_reconnect_wifi_task(void);

extern void start_wifi_ap(void);
extern esp_err_t start_wifi_sta(void);
