#ifndef __BT_H__
#define __BT_H__

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "common/bt_hci_common.h"

#include "esp_bt.h"
#include "badge.h"

#define BLE_SCAN_INTERVAL 0x50 // it will scan every X * 0,625ms
#define BLE_SCAN_WINDOW 0x30 // it will scan for X * 0,625ms

#define BLE_ADV_MIN 5 * 0x640 // X seconds * 0x640
#define BLE_ADV_MAX 5 * 0x640 // X seconds * 0x640

#define NODE_QUEUE_TIMEOUT_MS 20000

void bt_init();
void bt_task(void *);

#endif