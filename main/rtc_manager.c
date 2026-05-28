#include "rtc_manager.h"
#include "pins.h"
#include "i2c_manager.h"
#include "time_manager.h"
#include <esp_log.h>
#include <ds3231.h>
#include <freertos/semphr.h>

static const char *TAG = "RTC_MGR";

static i2c_dev_t ds3231_dev = {0};
static SemaphoreHandle_t rtc_mutex = NULL;
static bool rtc_available = false;

char current_timezone[64] = "EET-2EEST,M3.5.0/3,M10.5.0/4";   // Default Romania

// ====================== INIT ======================
esp_err_t rtc_manager_init(void)
{
    rtc_mutex = xSemaphoreCreateMutex();
    if (rtc_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }

    memset(&ds3231_dev, 0, sizeof(i2c_dev_t));
    ds3231_dev.timeout_ticks = I2CDEV_MAX_STRETCH_TIME;

    esp_err_t ret = ds3231_init_desc(&ds3231_dev, 0, I2C_SDA_PIN, I2C_SCL_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DS3231 init failed");
        return ret;
    }

    // Test citire
    struct tm test;
    if (rtc_manager_read_time(&test) == ESP_OK) {
        rtc_available = true;
        rtc_manager_set_system_time();          // Aplică TZ + DST = -1
        ESP_LOGI(TAG, "DS3231 OK → Time: %s", rtc_manager_get_formatted_time());
    }

    return ESP_OK;
}

// ====================== READ / WRITE ======================
esp_err_t rtc_manager_read_time(struct tm *timeinfo)
{
    if (!timeinfo) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(rtc_mutex, pdMS_TO_TICKS(600)) != pdTRUE)
        return ESP_ERR_TIMEOUT;

    esp_err_t ret = ds3231_get_time(&ds3231_dev, timeinfo);
    xSemaphoreGive(rtc_mutex);
    return ret;
}

esp_err_t rtc_manager_write_time(struct tm *timeinfo)
{
    if (!timeinfo) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(rtc_mutex, pdMS_TO_TICKS(600)) != pdTRUE)
        return ESP_ERR_TIMEOUT;

    esp_err_t ret = ds3231_set_time(&ds3231_dev, timeinfo);
    xSemaphoreGive(rtc_mutex);
    return ret;
}

// ====================== SET SYSTEM TIME (CRITICAL) ======================
esp_err_t rtc_manager_set_system_time(void)
{
    struct tm t;
    if (rtc_manager_read_time(&t) != ESP_OK) {
        return ESP_FAIL;
    }

    t.tm_isdst = -1;                    // Important - așa cum ai cerut

    time_t timestamp = mktime(&t);
    if (timestamp == -1) return ESP_FAIL;

    struct timeval tv = {.tv_sec = timestamp, .tv_usec = 0};
    settimeofday(&tv, NULL);

    // Aplicăm timezone
    setenv("TZ", current_timezone, 1);
    tzset();

    ESP_LOGI(TAG, "System time set from RTC: %s | TZ: %s", 
             rtc_manager_get_formatted_time(), current_timezone);

    return ESP_OK;
}

// ====================== FORMATTED TIME ======================
const char* rtc_manager_get_formatted_time(void)
{
    static char buf[32];
    struct tm t;
    if (rtc_manager_read_time(&t) == ESP_OK) {
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    } else {
        strcpy(buf, "RTC ERROR");
    }
    return buf;
}

bool rtc_manager_is_available(void)
{
    return rtc_available;
}