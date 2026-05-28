#include "time_manager.h"
#include "rtc_manager.h"
#include <esp_log.h>
#include <esp_sntp.h>
#include <string.h>

static const char* TAG = "TIME_MGR";
static bool sntp_initialized = false;

// ====================== SNTP Callback ======================
static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "SNTP synchronized successfully!");
    // rtc_manager_update_rtc_from_system();     // actualizăm și RTC-ul
    // event_bus_post(SYSTEM_EVENTS, EVENT_SNTP_SYNC_SUCCESS, NULL, 0);
}

// ====================== INIT ======================
esp_err_t time_manager_init(void)
{
    ESP_LOGI(TAG, "Time Manager initialized (using RTC as base)");
    return ESP_OK;
}

// ====================== SNTP ======================
esp_err_t time_manager_sync_with_sntp(void)
{
    if (sntp_initialized) {
        sntp_restart();
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting SNTP synchronization...");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    esp_sntp_init();
    sntp_initialized = true;

    return ESP_OK;
}

// ====================== GET TIME ======================
struct tm time_manager_get_current_time(void)
{
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return timeinfo;
}

time_t time_manager_get_timestamp(void)
{
    time_t now;
    time(&now);
    return now;
}

const char* time_manager_get_formatted_time(void)
{
    static char buf[16];
    struct tm t = time_manager_get_current_time();
    strftime(buf, sizeof(buf), "%H:%M:%S", &t);
    return buf;
}

const char* time_manager_get_formatted_datetime(void)
{
    static char buf[32];
    struct tm t = time_manager_get_current_time();
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    return buf;
}

const char* time_manager_get_formatted_date(void)
{
    static char buf[16];
    struct tm t = time_manager_get_current_time();
    strftime(buf, sizeof(buf), "%Y-%m-%d", &t);
    return buf;
}

bool time_manager_is_time_valid(void)
{
    struct tm t = time_manager_get_current_time();
    return (t.tm_year > 120);        // După anul 2020
}

// ====================== BROWSER ======================
esp_err_t time_manager_set_from_browser(struct timeval *tv)
{
    if (!tv) return ESP_ERR_INVALID_ARG;

    settimeofday(tv, NULL);
    
    // Actualizăm și RTC-ul
    struct tm timeinfo;
    localtime_r(&tv->tv_sec, &timeinfo);
    rtc_manager_write_time(&timeinfo);

    ESP_LOGI(TAG, "Time updated from browser: %s", time_manager_get_formatted_datetime());
    return ESP_OK;
}