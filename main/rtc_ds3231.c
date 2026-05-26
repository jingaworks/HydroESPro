#include "config.h"
#include "rtc_ds3231.h"
#include "time_manager.h"

static const char *TAG = "rtc_ds3231";


i2c_dev_t ds3231_rtc;                  // ← Global low-level device


esp_err_t init_rtc_ds3231(void)
{
    ESP_LOGI(TAG, "Init RTC Time");
    memset(&ds3231_rtc, 0, sizeof(i2c_dev_t));

    ds3231_rtc.timeout_ticks = I2CDEV_MAX_STRETCH_TIME;

    ESP_ERROR_CHECK(ds3231_init_desc(&ds3231_rtc, 0, I2C_MASTER_SDA, I2C_MASTER_SCL));
    return rtc_ds3231_set_system_time();
}

esp_err_t rtc_ds3231_set_system_time(void)
{
    if (ds3231_get_time(&ds3231_rtc, &rtc_time) != ESP_OK) {
        printf("Could not get DS3231 RTC time\n");
        return ESP_FAIL;
    }
    
    time_synchronized = 1;
    
    // 1. Setează fusul orar mai întâi (Corecția anterioară, foarte importantă)
    time_manager_set_timezone(current_timezone);
    
    // 2. !!! NOUĂ CORECȚIE CRITICĂ PENTRU ORA DE VARĂ !!!
    // Îi spunem sistemului să decidă automat DST-ul în funcție de data de 23 Mai
    rtc_time.tm_isdst = -1; 
    
    // 3. Acum mktime va calcula la secundă timestamp-ul fără să mai adauge sau să scadă o oră de la el
    time_t time_sec = mktime(&rtc_time);
    
    ESP_LOGI(TAG, "Setting System Time: %s", asctime(&rtc_time));
    
    struct timeval now = {.tv_sec = time_sec};
    settimeofday(&now, NULL);
    
    return ESP_OK;
}


esp_err_t rtc_ds3231_set_module_time(struct tm timeinfo)
{
    if (ds3231_set_time(&ds3231_rtc, &timeinfo) != ESP_OK) {
        return ESP_FAIL;
    }
    time_synchronized = true;
    return ESP_OK;
}

esp_err_t set_ds_browser_time(struct timeval *tv)
{
    time_t now = (time_t)tv->tv_sec;
    struct tm timeinfo;
    
    localtime_r(&now, &timeinfo);
    
    if (rtc_ds3231_set_module_time(timeinfo) == ESP_OK){
        return rtc_ds3231_set_system_time();
    }

    return ESP_FAIL;
}


