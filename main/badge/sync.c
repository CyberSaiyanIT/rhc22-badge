#include "sync.h"

static int64_t last_run = 0;
static int64_t current_run = 0;
static bool errors, forced, connected = false;

esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    static int output_len = 0;       // Stores number of bytes read
    FILE *fp;

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(__FILE__, "HTTP_EVENT_ERROR");
            output_len = 0;
            errors = true;
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(__FILE__, "HTTP_EVENT_ON_CONNECTED");

            if(!connected) connected = true;
            output_len = 0;
            errors = false;

            fp = fopen(TMP_SCHEDULE_FILE, "w");
            fclose(fp);

            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(__FILE__, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(__FILE__, "HTTP_EVENT_ON_HEADER");
            ESP_LOGI(__FILE__, "%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(__FILE__, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                ESP_LOGI(__FILE__, "%d", evt->data_len);
            }
            
            if(errors)
                return ESP_FAIL;
            
            fp = fopen(TMP_SCHEDULE_FILE, "a");
            fwrite (evt->data, sizeof(char), evt->data_len, fp);
            fclose(fp);

            output_len += evt->data_len;

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(__FILE__, "HTTP_EVENT_ON_FINISH");
            ESP_LOGI(__FILE__, "Saving %d bytes to schedule", output_len);

            if(errors)
                return ESP_FAIL;

            if(!esp_http_client_is_complete_data_received(evt->client)){
                errors = true;
                return ESP_FAIL;
            }
                
            struct stat st;
            if (stat(SCHEDULE_FILE, &st) == 0) {
                // Delete it if it exists
                unlink(SCHEDULE_FILE);
            }

            // Rename original file
            ESP_LOGI(__FILE__, "Renaming file");
            if (rename(TMP_SCHEDULE_FILE, SCHEDULE_FILE) != 0) {
                ESP_LOGE(__FILE__, "Rename failed");
                return ESP_FAIL;
            }

            ui_event_load(); // Preload in UI
            ESP_LOGI(__FILE__, "File saved %d bytes", output_len);
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(__FILE__, "HTTP_EVENT_DISCONNECTED");
            
            output_len = 0;
            connected = false;
            ui_toggle_sync();
            
            if(errors){
                errors = false;
                return ESP_FAIL;
            }
                
            last_run = current_run; // update timer
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(__FILE__, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

void schedule_sync_handler(bool force) {
    current_run = esp_timer_get_time();
    forced = force;
    ESP_LOGI(__FILE__, "Previous time: %lld", last_run);
    ESP_LOGI(__FILE__, "Current time: %lld", current_run);
    if(!connected && (forced || last_run == 0 || (current_run - last_run) > 1000*SYNC_PERIOD_MS))
    {
        ESP_LOGI(__FILE__, "Connecting to https://cybersaiyan.it/esc");

        esp_http_client_config_t http_config = {
        .url = "https://cybersaiyan.it/esc",
        .event_handler = _http_event_handle,
        };
        esp_http_client_handle_t http_client = esp_http_client_init(&http_config);
        esp_http_client_set_header(http_client, "Content-Type", "application/json");
        esp_err_t err = esp_http_client_perform(http_client);

        if (err == ESP_OK) {
        ESP_LOGI(__FILE__, "Status = %d, content_length = %" PRId64,
                esp_http_client_get_status_code(http_client),
                esp_http_client_get_content_length(http_client));
        }
        if(!esp_http_client_cleanup(http_client)){
            ESP_LOGI(__FILE__, "client cleaned up");
        }
    }
}
