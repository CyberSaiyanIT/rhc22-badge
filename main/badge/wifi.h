#ifndef WIFI__H
#define WIFI__H

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "badge.h"

#define AP_INACTIVITY_TIMEOUT_S 60
#define AP_MAX_STA_CONN 5
#define STA_TIMEOUT_MS 20000
#define STA_MAXIMUM_RETRY 5

void wifi_init(void);

bool start_wifi_ap(void);
bool start_wifi_sta(void);
bool start_wifi_apsta(void);

void stop_wifi(void);

void wifi_task(void *);

#endif