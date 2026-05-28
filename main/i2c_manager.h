// i2c_manager.h
#pragma once

#include "esp_err.h"
#include "driver/i2c.h"

esp_err_t i2c_manager_init(int sda_pin, int scl_pin, uint32_t clock_speed);

esp_err_t i2c_manager_write_byte(uint8_t dev_addr, uint8_t data);
esp_err_t i2c_manager_read_byte(uint8_t dev_addr, uint8_t *data);

esp_err_t i2c_manager_write(uint8_t dev_addr, uint8_t *data, size_t len);
esp_err_t i2c_manager_read(uint8_t dev_addr, uint8_t *data, size_t len);

esp_err_t i2c_manager_scan(void);   // Pentru debug - listează device-urile I2C