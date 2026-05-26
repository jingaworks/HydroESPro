#include "time_manager.h"
#include "rtc_ds3231.h"

#include "utill.h"                    // pentru get_time_sec()

#include <esp_log.h>
#include <sys/time.h>                 // pentru gettimeofday
#include <esp_sntp.h>                 // pentru gettimeofday
#include <string.h>

#include <time.h>
#include <sys/time.h>

#include "esp_sntp.h"

#include "event_bus.h"

// Legacy global variables (kept temporarily for compatibility)
struct tm rtc_time = { 0 };
bool time_synchronized = false;


const char *TIME_MANAGER_TAG = "TIME_MANAGER";   // non-static

char current_timezone[64] = "EET-2EEST-3,M3.5.0/03:00:00,M10.5.0/04:00:00";
static bool time_synced = false;
static TimerHandle_t xTimerRTC = NULL;

/* ==================== INTERNAL ==================== */

static void time_manager_publish_update(void) {
    time_event_data_t event_data = { 0 };

    event_data.timeinfo = rtc_time;
    event_data.unix_time = time_manager_get_unix_time();
    event_data.is_synced = time_synced;
    event_data.is_dst = rtc_time.tm_isdst > 0;

    event_bus_post(SYSTEM_EVENTS, SYS_EVENT_TIME_UPDATED, &event_data, sizeof(event_data));
}

// de we need it?
static void vRTCTimerCallback(TimerHandle_t pxTimer) {
    time_t now = (time_t)get_time_sec();
    localtime_r(&now, &rtc_time);
    time_manager_publish_update();
}

static void time_sync_notification_cb(struct timeval *tv) {
    time_t now = (time_t)tv->tv_sec;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    time_manager_set_timezone(current_timezone);

    if (rtc_ds3231_set_module_time(timeinfo) == ESP_OK) {
        rtc_time = timeinfo;
        time_synced = true;

        ESP_LOGI(TIME_MANAGER_TAG, "Time successfully synchronized via SNTP");

        time_manager_publish_update();
        event_bus_post(SYSTEM_EVENTS, SYS_EVENT_TIME_SYNCED, NULL, 0);
    }
}

void initialize_sntp(void) {
    // verifica daca avem conectivitatea la internet
    ESP_LOGI(TIME_MANAGER_TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
}

/* ==================== PUBLIC API ==================== */

esp_err_t time_manager_init(void) {
    ESP_LOGI(TIME_MANAGER_TAG, "Initializing Time Manager...");

    // Această inițializare va apela rtc_ds3231_set_system_time(), 
    // care acum setează corect fusul orar înainte de a stabili ora sistemului!

    // ESP_ERROR_CHECK(init_rtc_ds3231());

#ifdef LWIP_DHCP_GET_NTP_SRV
    sntp_servermode_dhcp(1);
#endif
    // verificam daca avem conectivitatea la internet
    initialize_sntp();

    // do we need it?
    xTimerRTC = xTimerCreate("RTC_Timer", pdMS_TO_TICKS(1000), pdTRUE, 0, vRTCTimerCallback);
    if (xTimerRTC != NULL) {
        xTimerStart(xTimerRTC, 0);
    }
    else {
        ESP_LOGE(TIME_MANAGER_TAG, "Failed to create RTC update timer");
    }

    time_manager_publish_update();
    return ESP_OK;
}

esp_err_t time_manager_set_timezone(const char *tz) {
    if (tz == NULL) return ESP_ERR_INVALID_ARG;
    strncpy(current_timezone, tz, sizeof(current_timezone) - 1);
    current_timezone[sizeof(current_timezone) - 1] = '\0';

    setenv("TZ", current_timezone, 1);
    tzset();
    ESP_LOGI(TIME_MANAGER_TAG, "Timezone updated to: %s", current_timezone);
    return ESP_OK;
}

const char *time_manager_get_timezone(void) {
    return current_timezone;
}

esp_err_t time_manager_set_time(struct tm *timeinfo) {
    if (timeinfo == NULL) return ESP_ERR_INVALID_ARG;

    if (rtc_ds3231_set_module_time(*timeinfo) == ESP_OK) {
        rtc_time = *timeinfo;
        time_synced = true;
        time_manager_publish_update();
        return ESP_OK;
    }
    return ESP_FAIL;
}

time_t time_manager_get_unix_time(void) {
    struct timeval tv = { 0 };
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}

struct tm time_manager_get_tm(void) {
    return rtc_time;
}

bool time_manager_is_synced(void) {
    return time_synced;
}

void time_manager_print_current_time(void) {
    ESP_LOGI(TIME_MANAGER_TAG, "Current time: %02d/%02d/%04d %02d:%02d:%02d",
        rtc_time.tm_mday, rtc_time.tm_mon + 1, rtc_time.tm_year + 1900,
        rtc_time.tm_hour, rtc_time.tm_min, rtc_time.tm_sec);
}