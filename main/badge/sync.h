#ifndef _SCHEDULE_H
#define _SCHEDULE_H

#include <esp_err.h>
#include <esp_http_client.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "badge.h"

#define SYNC_PERIOD_MS 30 * 60 * 1000

void schedule_sync_handler(bool force);

#endif // _SCHEDULE_H
