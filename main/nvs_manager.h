#pragma once

#include <nvs_flash.h>
#include <nvs.h>
#include <esp_log.h>
#include <esp_err.h>
#include "nvs_manager.h"

#include "config.h"



extern esp_err_t erase_wifi_config(void);
extern esp_err_t save_wifi_config(void);
extern esp_err_t load_wifi_config(void);

extern esp_err_t erase_board_config(void);
extern esp_err_t save_board_config(void);
extern esp_err_t load_board_config(void);

extern esp_err_t erase_hydro_config(void);
extern esp_err_t save_hydro_config(void);
extern esp_err_t load_hydro_config(void);

extern esp_err_t erase_alarm_config(void);
extern esp_err_t save_alarm_config(void);
extern esp_err_t load_alarm_config(void);

extern esp_err_t erase_log_config(void);
extern esp_err_t save_log_config(void);
extern esp_err_t load_log_config(void);

extern void nvs_manager_load_all_config(void);