#pragma once



#define RELEU_PORNIT    0  
#define RELEU_OPRIT     1

#define LED_PORNIT      1
#define LED_OPRIT       0

/* Pin definitions */

#define BOARD_GPIO_2 2      // used by sd card
#define BOARD_GPIO_4 4
#define BOARD_GPIO_5 5      // must be HIGH during boot | used by leds
#define BOARD_GPIO_12 12    // must be LOW during boot
#define BOARD_GPIO_13 13
#define BOARD_GPIO_14 14    // used by sd card
#define BOARD_GPIO_15 15    // used by sd card
#define BOARD_GPIO_16 16    // used by dht
#define BOARD_GPIO_17 17    // free
#define BOARD_GPIO_18 18    // used by leds
#define BOARD_GPIO_19 19    // used by leds
#define BOARD_GPIO_21 21    // used by i2c
#define BOARD_GPIO_22 22    // used by i2c
#define BOARD_GPIO_23 23    // free
#define BOARD_GPIO_25 25    // used by relays
#define BOARD_GPIO_26 26    // used by relays
#define BOARD_GPIO_27 27    // used by buzzer
#define BOARD_GPIO_32 32    // used by relays
#define BOARD_GPIO_33 33    // used by relays
#define BOARD_GPIO_34 34    // used by button
#define BOARD_GPIO_35 35    // input only | free
#define BOARD_GPIO_36 36    // input only | free
#define BOARD_GPIO_39 39    // input only | free


// empty pins
// 2**, 4*, 5*, 12, 13, 14**, 15**, 16*, 17, 18*, 19*, 21*, 22*, 23, 25*, 26*, 27*, 32*, 33*, 34*, 35, 36, 39 // 14 used


// #define BOARD_GPIO_25 25
// #define BOARD_GPIO_26 26
// #define BOARD_GPIO_27 27
// #define BOARD_GPIO_32 32
// #define BOARD_GPIO_33 33


// i2c
#define I2C_MASTER_SDA          BOARD_GPIO_21
#define I2C_MASTER_SCL          BOARD_GPIO_22

// relays
#define RELAY_WATER_PUMP_GPIO   BOARD_GPIO_25
#define RELAY_AIR_PUMP_GPIO     BOARD_GPIO_26

// buzzer
#define BUZZER_GPIO             BOARD_GPIO_27

// dht
#define SENSOR_DHT_GPIO         BOARD_GPIO_16

// leds
#define WARNING_LED_GPIO        BOARD_GPIO_5
#define AIR_PUMP_LED_GPIO       BOARD_GPIO_18
#define WATER_PUMP_LED_GPIO     BOARD_GPIO_19

// button
#define BUTTON_GPIO             BOARD_GPIO_34


