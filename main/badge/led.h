#ifndef _LED_H
#define _LED_H

#include <stdint.h>
#include <stdio.h>

#include "driver/i2c.h"
#include "rgb.h"
#include "badge.h"

#define I2C_MASTER_SCL_IO           0
#define I2C_MASTER_SDA_IO           1
#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          400000
#define I2C_MASTER_TIMEOUT_MS       1000

#define MAGENTA_SAIYAN 0xE80B60

enum LED_COLOR {
    RED, GREEN, BLUE,
};

void led_init();

void set_screen_led_backlight(uint8_t);

void led_task(void* arg);

#endif // _LED_H
