#ifndef __SPIFFS_H__
#define __SPIFFS_H__

#include <stdio.h>
#include <string.h>

#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "esp_err.h"
#include "esp_log.h"

void nvs_init();
void spiffs_init();

#endif