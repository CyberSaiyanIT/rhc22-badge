#ifndef _UI_H
#define _UI_H

#include <stdio.h>
#include <stdlib.h>

#include "lvgl.h"
#include "lvgl_helpers.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "button.h"

#define BUTTON_1 0x08 // DOWN button
#define BUTTON_2 0x09 // UP button

#include "badge.h"
#include "snake.h"

#define SCREEN_BRIGHT_MAX 96
#define SCREEN_BRIGHT_MID 32
#define SCREEN_BRIGHT_OFF 0

#define BRIGHT_MID_TIMEOUT_MS 5000
#define BRIGHT_OFF_TIMEOUT_MS 15000

#define ADMIN_STATE_OFF 0
#define ADMIN_STATE_AP 1
#define ADMIN_STATE_STA 2

lv_obj_t *screen_logo;  // page 0
lv_obj_t *screen_event; // page 1
lv_obj_t *screen_radar; // page 2
lv_obj_t *screen_rssi;  // page 3
lv_obj_t *screen_snake; // page 4
lv_obj_t *screen_admin; // page 5

lv_task_t* snake_task_handle;
lv_task_t* radar_task_handle;
lv_task_t* rssi_task_handle;
lv_task_t* backlight_task_handle;

void ui_task(void *);
void button_task(void *arg);

void ui_event_load();
void ui_toggle_sync();
void ui_connection_progress(uint8_t cur, uint8_t max);

// void ui_button_up();
// void ui_button_down();
// void ui_switch_page_up();
// void ui_switch_page_down();
#endif