// i2c_manager.c
#include "i2c_manager.h"
#include "pins.h"
#include <esp_log.h>
#include <driver/i2c.h>

static const char* TAG = "I2C_MGR";
static i2c_port_t i2c_port = I2C_NUM_0;

esp_err_t i2c_manager_init(int sda_pin, int scl_pin, uint32_t clock_speed)
{
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = clock_speed,
    };

    esp_err_t ret = i2c_param_config(i2c_port, &i2c_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure I2C parameters");
        return ret;
    }

    ret = i2c_driver_install(i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2C driver");
        return ret;
    }

    ESP_LOGI(TAG, "I2C Manager initialized (SDA:%d, SCL:%d, Speed:%d Hz)", 
             sda_pin, scl_pin, clock_speed);
    return ESP_OK;
}

// ====================== WRITE ======================
esp_err_t i2c_manager_write_byte(uint8_t dev_addr, uint8_t data)
{
    return i2c_manager_write(dev_addr, &data, 1);
}

esp_err_t i2c_manager_write(uint8_t dev_addr, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

// ====================== READ ======================
esp_err_t i2c_manager_read_byte(uint8_t dev_addr, uint8_t *data)
{
    return i2c_manager_read(dev_addr, data, 1);
}

esp_err_t i2c_manager_read(uint8_t dev_addr, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

// ====================== SCAN ======================
esp_err_t i2c_manager_scan(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus...");
    int devices = 0;

    for (uint8_t addr = 0x03; addr < 0x78; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Device found at 0x%02X", addr);
            devices++;
        }
    }

    ESP_LOGI(TAG, "I2C scan completed. %d device(s) found.", devices);
    return ESP_OK;
}