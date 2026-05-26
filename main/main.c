#include <stdio.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <i2cdev.h>


#include "board.h"
#include "wifi_conn.h"
#include "http_server.h"
#include "lcd_display.h"
#include "rtc_ds3231.h"
#include "dht_sensor.h"
#include "ds18b20.h"
#include "utill.h"
// #include "custom_schs.h"

#include "event_bus.h"
#include "state_manager.h"
#include "time_manager.h"
#include "storage_manager.h"
#include "hydro_manager.h"

#include "button.h"
#include "buzzer.h"

#include "relayS.h"

// #include "speeds.h"
// #include "relay_timer.h"

static const char *TAG = "MAIN";


void app_main(void)
{
    ESP_LOGI(TAG, "Start APP...");

    // vTaskDelay(3000 / portTICK_PERIOD_MS);

    /****************** LOG Setup ********************/
    esp_log_level_set("*", ESP_LOG_INFO);
    // esp_log_level_set("*", ESP_LOG_DEBUG);
    // esp_log_level_set("*", ESP_LOG_VERBOSE);
    // esp_log_level_set("WEBSOCKET_CLIENT", ESP_LOG_VERBOSE);
    // esp_log_level_set("TRANSPORT_WS", ESP_LOG_VERBOSE);
    // esp_log_level_set("TRANS_TCP", ESP_LOG_VERBOSE);

    // TODO: Setare pin intro funcție
    // gpio_pullup_en(RELAY_AIR_PUMP_GPIO);
    // gpio_set_direction(RELAY_AIR_PUMP_GPIO, GPIO_MODE_OUTPUT);

    // gpio_pullup_en(RELAY_WATER_PUMP_GPIO);
    // gpio_set_direction(RELAY_WATER_PUMP_GPIO, GPIO_MODE_OUTPUT);
    
    gpio_pulldown_en(WARNING_LED_GPIO);
    gpio_set_direction(WARNING_LED_GPIO, GPIO_MODE_OUTPUT);

    gpio_pulldown_en(AIR_PUMP_LED_GPIO);
    gpio_set_direction(AIR_PUMP_LED_GPIO, GPIO_MODE_OUTPUT);

    gpio_pulldown_en(WATER_PUMP_LED_GPIO);
    gpio_set_direction(WATER_PUMP_LED_GPIO, GPIO_MODE_OUTPUT);

    // gpio_pulldown_en(BUZZER_GPIO);
    // gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);

    
    /****************** Init NVS *********************/
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /******************** Load config *****************/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Loading config...");
    load_all_config();
    
    /******************** Init I2C ********************/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Init I2C...");
    ESP_ERROR_CHECK(i2cdev_init());
    
    /********** Init and get rtc_ds3231 time **********/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Init RTC Time");
    ESP_ERROR_CHECK(init_rtc_ds3231());

    /**************** Init Event bus ******************/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Init Event Bus");
    event_bus_init();

    
    /****************** Start WiFi ********************/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Start WiFi...");
    if (start_wifi_sta() == ESP_OK) {
        ESP_LOGI(TAG, "Start WiFi Station Started");
    }
    // start_wifi_ap();


    /*********** Time Manager (RTC + SNTP) ************/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Init Time Manager...");
    ESP_ERROR_CHECK(time_manager_init());
    
    
    /************ Init SD Card & Storage **************/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Init Storage Manager...");
    ESP_ERROR_CHECK(storage_manager_init());


    /**************** Init State Manager **************/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Init State Manager...");
    ESP_ERROR_CHECK(state_manager_init());
    

    /******************* Init Relays ******************/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Init Relays...");
    relays_init();


    /******************* Init Buzzer ******************/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Init Buzzer...");
    buzzer_init();


    /******************* Init Display *****************/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Init Display...");
    init_lvgl_lcd_display();


    /****************** Start DHT task ****************/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Start DHT task...");
    start_dht_task();

    
    /****************** Start DST task ****************/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Start DST task...");
    start_ds18b20_task();

    
    /********** Start Control Hydroponics task ********/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Start Control Hydroponics task...");
    start_control_hydroponics_task();
    

    /******************* Init Button ******************/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Init Button...");
    button_init();


    /*************** Start Hydro Tasks ****************/
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "State Manager Set State Normal...");
    state_manager_set_state(SYS_STATE_NORMAL, "APP Started");
    

    /****************** beep Buzzer *******************/
    buzzer_beep_direct(500); 
}
