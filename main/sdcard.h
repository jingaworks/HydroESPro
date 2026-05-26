#pragma once

#include <string.h>
#include <stdint.h>
#include <esp_err.h>
#include <esp_log.h>
#include <nvs.h>

#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include <dirent.h>



#define EXAMPLE_MAX_CHAR_SIZE    64
#define MOUNT_POINT_SD      "/sdcard"


extern esp_err_t init_sdcard_sdmmc(void);
extern esp_err_t init_sdcard(void);
