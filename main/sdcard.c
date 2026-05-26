#include "sdcard.h"



static const char *TAG = "SD CARD";


#define SD_CARD_DIRS_NUM 3

typedef struct dirs_exist_t {
    char name[7];
    bool exist;
} dirs_exist_t;

dirs_exist_t sdcard_dirs[SD_CARD_DIRS_NUM] = {
  {"logs", false},
  {"info", false},
  {"errors", false}
};

/***********************************************/
/*** SD card preferences *************************/
static sdmmc_host_t host = SDMMC_HOST_DEFAULT();
static const char mount_point[] = MOUNT_POINT_SD;
sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

// Options for mounting the filesystem.
// If format_if_mount_failed is set to true, SD card will be partitioned and
// formatted in case when mounting fails.
static esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = true,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024};
sdmmc_card_t *card;

static void check_dirs(void)
{
  ESP_LOGI(TAG, "check_dirs()");

  char * dirpath = "/sdcard";
  struct dirent *entry;
  
  // check if there are any directors...
  DIR *dir = opendir(dirpath);
  if (!dir) {
    ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath); 
  }

  /* Iterate over all folders and fetch their names */
  while ((entry = readdir(dir)) != NULL) {
    for (size_t i = 0; i < SD_CARD_DIRS_NUM; i++) {
      if (!strcmp(sdcard_dirs[i].name, entry->d_name)) {
        sdcard_dirs[i].exist = true;
        break;
      }
    }
  }
  
  int ret;
  for (size_t i = 0; i < SD_CARD_DIRS_NUM; i++) {
    if (!sdcard_dirs[i].exist) {
      ESP_LOGI(TAG, "mkdir %s", sdcard_dirs[i].name);
      char dir_name[64];
      sprintf(dir_name, "/sdcard/%s", sdcard_dirs[i].name);
      ret = mkdir(dir_name, 0775);
      ESP_LOGI(TAG, "mkdir %s %s", sdcard_dirs[i].name, (!ret)? "Success": "Fail" );
    }
  }

  /* Iterate over all files / folders and fetch their names and sizes */
  while ((entry = readdir(dir)) != NULL) {
    ESP_LOGI(TAG, "no: %u \tname: %s \ttype: %s", entry->d_ino, entry->d_name, (entry->d_type == 2)? "Dir": "Unknown");
  }
}

esp_err_t init_sdcard_sdmmc(void)
{
  esp_err_t ret;

  ESP_LOGI(TAG, "Initializing SD card");
  ESP_LOGI(TAG, "Using SDMMC peripheral");

  // This initializes the slot without card detect (CD) and write protect (WP) signals.
  // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
  slot_config.width = 1;

  // Enable internal pullups on enabled pins. The internal pullups
  // are insufficient however, please make sure 10k external pullups are
  // connected on the bus. This is for debug / example purpose only.
  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  ESP_LOGI(TAG, "Mounting filesystem");
  ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount filesystem. "
                  "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
    } 
    else {
      ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                  "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
    }
    return ret;
  }
  ESP_LOGI(TAG, "Filesystem mounted");

  // Card has been initialized, print its properties
  sdmmc_card_print_info(stdout, card);
  
  check_dirs();
  
  return ret;
}

esp_err_t init_sdcard(void)
{
  return init_sdcard_sdmmc();
}
