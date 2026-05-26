#include "storage_manager.h"
// #include "sdcard.h"           // păstrăm low-level pentru moment
#include "utill.h"
#include <esp_log.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>


// #include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
// #include <dirent.h>

static const char *TAG = "STORAGE_MANAGER";

static bool sd_mounted = false;

// Legacy array (păstrat temporar)
const char log_types[7][7] = {
    "log", "reset", "dht", "ds", "error", "system", ""
};

// Queue
static QueueHandle_t log_data_queue = NULL;

typedef struct {
    log_type_t type;
    char dir[32];
    char *data;
    size_t len;
} log_data_t;

// =============================================
// SD CARD LOW-LEVEL INITIALIZATION (mutat din sdcard.c)
// =============================================

static sdmmc_host_t host = SDMMC_HOST_DEFAULT();
static sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
static esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = true,
    .max_files = 8,
    .allocation_unit_size = 16 * 1024
};
static sdmmc_card_t *card = NULL;

static esp_err_t storage_init_sdcard_hw(void) {
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing SD card (SDMMC)...");

    slot_config.width = 1;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT_SD, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                "If you want formatting, set format_if_mount_failed = true");
        }
        else {
            ESP_LOGE(TAG, "Failed to initialize SD card (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    sd_mounted = true;
    sdmmc_card_print_info(stdout, card);

    ESP_LOGI(TAG, "SD Card mounted successfully on %s", MOUNT_POINT_SD);
    return ESP_OK;
}

// =============================================
// INTERNAL FUNCTIONS
// =============================================

static esp_err_t storage_create_directories(void) {
    const char *dirs[] = { MOUNT_POINT_LOGS, MOUNT_POINT_INFO, MOUNT_POINT_ERRORS };

    for (int i = 0; i < 3; i++) {
        struct stat st = { 0 };
        if (stat(dirs[i], &st) == -1) {
            if (mkdir(dirs[i], 0775) != 0) {
                ESP_LOGE(TAG, "Failed to create directory: %s", dirs[i]);
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "Created directory: %s", dirs[i]);
        }
    }
    return ESP_OK;
}

static void sd_log_writer_task(void *arg) {
    ESP_LOGI(TAG, "SD Log Writer Task started");

    log_data_t *log_item;
    char filepath[128];

    for (;;) {
        if (xQueueReceive(log_data_queue, &log_item, portMAX_DELAY) == pdTRUE) {
            // ESP_LOGI(TAG, "Received log item: %s", log_types[log_item->type]);
            if (log_item && log_item->data) {
                // Build path using time_manager
                struct tm tm = time_manager_get_tm();
                snprintf(filepath, sizeof(filepath), "%s/%s_%02d_%02d_%04d.txt",
                    log_item->dir,
                    log_types[log_item->type],
                    tm.tm_mday,
                    tm.tm_mon + 1,
                    tm.tm_year + 1900);

                ESP_LOGI(TAG, "Writing log item to file: %s", filepath);

                FILE *f = fopen(filepath, "a+");
                if (f) {
                    fwrite(log_item->data, 1, log_item->len, f);
                    fclose(f);

                    ESP_LOGI(TAG, "Wrote log item to file: %s", filepath);
                }
                else {
                    ESP_LOGE(TAG, "Failed to open log file: %s", filepath);
                }


                free(log_item->data);
                free(log_item);
            }
        }
    }
}

// =============================================
// PUBLIC API
// =============================================

esp_err_t storage_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Storage Manager...");

    // 1. Initialize SD Card hardware
    ESP_ERROR_CHECK(storage_init_sdcard_hw());

    // 2. Create required directories
    ESP_ERROR_CHECK(storage_create_directories());

    // 3. Create logging queue
    log_data_queue = xQueueCreate(LOG_DATA_QUEUE_SIZE, sizeof(log_data_t *));
    if (log_data_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create log data queue");
        return ESP_FAIL;
    }

    // 4. Start log writer task
    xTaskCreate(sd_log_writer_task, "sd_writer", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Storage Manager initialized successfully ✓");
    return ESP_OK;
}

esp_err_t storage_log_write(log_type_t type, const char *data, size_t len) {
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;

    // ESP_LOGI(TAG, "storage_log_write: Writing log of type: %s", log_types[type]);

    log_data_t *item = calloc(1, sizeof(log_data_t));
    if (!item) return ESP_ERR_NO_MEM;

    item->type = type;
    item->len = len;
    item->data = malloc(len);
    if (!item->data) {
        free(item);
        return ESP_ERR_NO_MEM;
    }

    memcpy(item->data, data, len);
    strcpy(item->dir, (type == LOG_TYPE_RESET || type == LOG_TYPE_SYSTEM)
        ? MOUNT_POINT_INFO : MOUNT_POINT_LOGS);

    if (xQueueSend(log_data_queue, &item, LOG_MAXDELAY) != pdTRUE) {
        free(item->data);
        free(item);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t storage_log_printf(log_type_t type, const char *format, ...) {
    // ESP_LOGI(TAG, "storage_log_printf: Writing log of type: %s", log_types[type]);
    char buffer[512];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len > 0) {
        return storage_log_write(type, buffer, len);
    }
    return ESP_FAIL;
}

bool storage_is_mounted(void) {
    return sd_mounted;
}
