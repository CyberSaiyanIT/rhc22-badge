#include "wifi.h"

static const char *TAG = "WIFI";
static wifi_mode_t curr_mode = WIFI_MODE_NULL;
QueueHandle_t wifi_queue;

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
const int FAIL_BIT = BIT1;

static int retry_num = 0;
static int ap_clients_num = 0;
static esp_timer_handle_t inactivity_timer;

static void inactivity_timer_callback(void* arg)
{
	if(!ap_clients_num){
		stop_wifi();
	} else {
		ESP_LOGE(__FILE__, "Timer should not be running...");
	}
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
		ap_clients_num++;
		esp_timer_stop(inactivity_timer);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
		ap_clients_num--;
		if(ap_clients_num < 0) ap_clients_num = 0;
		if(!ap_clients_num) esp_timer_start_once(inactivity_timer, AP_INACTIVITY_TIMEOUT_S * 1000000);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < STA_MAXIMUM_RETRY) {
			ui_connection_progress(retry_num+1, STA_MAXIMUM_RETRY);
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP (%d/%d)", retry_num, STA_MAXIMUM_RETRY);
			xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        } else {
            xEventGroupSetBits(wifi_event_group, FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		retry_num = 0;
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    } 
}

void wifi_init(void)
{
	esp_log_level_set("wifi", ESP_LOG_WARN);
	static bool initialized = false;
	if (initialized) {
		return;
	}
	ESP_ERROR_CHECK(esp_netif_init());
	wifi_event_group = xEventGroupCreate();
	
	esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
	assert(ap_netif);
	esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
	assert(sta_netif);
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
	ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &event_handler, NULL) );
	ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &event_handler, NULL) );
	// ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &event_handler, NULL) );


	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );
	ESP_ERROR_CHECK( esp_wifi_start() );

    curr_mode = WIFI_MODE_NULL;

	initialized = true;
}

bool start_wifi_ap(void)
{
	ESP_LOGI(__FILE__, "free_heap_size = %lu\n", esp_get_free_heap_size());
	
	const char* AP_WIFI_SSID = badge_obj.ap_ssid;
	const char* AP_WIFI_PASSWORD = badge_obj.ap_password;
	
	wifi_config_t wifi_config = { 0 };
	snprintf((char*)wifi_config.ap.ssid, SIZEOF(wifi_config.ap.ssid), "%s", AP_WIFI_SSID);
	snprintf((char*)wifi_config.ap.password, SIZEOF(wifi_config.ap.password), "%s", AP_WIFI_PASSWORD);
	wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
	wifi_config.ap.ssid_len = strlen(AP_WIFI_SSID);
	wifi_config.ap.max_connection = AP_MAX_STA_CONN;

	if (strlen(AP_WIFI_PASSWORD) == 0) {
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}


	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_AP) );
	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config) );
	ESP_ERROR_CHECK( esp_wifi_start() );
	ESP_ERROR_CHECK( esp_wifi_set_inactive_time(WIFI_IF_AP, AP_INACTIVITY_TIMEOUT_S) );

	const esp_timer_create_args_t timer_args = {
		.callback = &inactivity_timer_callback,
		.name = "inactivity-timer"
	};

	ESP_ERROR_CHECK(esp_timer_create(&timer_args, &inactivity_timer));

	ESP_LOGI(TAG, "WIFI_MODE_AP started. SSID:%s password:%s",
			 AP_WIFI_SSID, AP_WIFI_PASSWORD);

    curr_mode = WIFI_MODE_AP;
	ESP_LOGI(__FILE__, "free_heap_size = %lu\n", esp_get_free_heap_size());

	return ESP_OK;
}

bool start_wifi_sta()
{
	ESP_LOGI(__FILE__, "free_heap_size = %lu\n", esp_get_free_heap_size());
	
	const char* STA_WIFI_SSID = badge_obj.sta_ssid;
	const char* STA_WIFI_PASSWORD = badge_obj.sta_password;

	wifi_config_t wifi_config = { 0 };
	snprintf((char*)wifi_config.sta.ssid, SIZEOF(wifi_config.sta.ssid), "%s", STA_WIFI_SSID);
	snprintf((char*)wifi_config.sta.password, SIZEOF(wifi_config.sta.password), "%s", STA_WIFI_PASSWORD);

	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
	ESP_ERROR_CHECK( esp_wifi_connect() );

	int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
								   pdFALSE, pdTRUE, STA_TIMEOUT_MS / portTICK_PERIOD_MS);
	ESP_LOGI(TAG, "bits=%x", bits);
	if (bits & CONNECTED_BIT) {
		ESP_LOGI(TAG, "WIFI_MODE_STA connected. SSID:%s password:%s",
			 STA_WIFI_SSID, STA_WIFI_PASSWORD);
        schedule_sync_handler(true);
	} else {
		ESP_LOGI(TAG, "WIFI_MODE_STA can't connected. SSID:%s password:%s",
			 STA_WIFI_SSID, STA_WIFI_PASSWORD);
		stop_wifi();
	}

    curr_mode = WIFI_MODE_STA;
	ESP_LOGI(__FILE__, "free_heap_size = %lu\n", esp_get_free_heap_size());

	return (bits & CONNECTED_BIT) != 0;
}

bool start_wifi_apsta()
{
	ESP_LOGI(__FILE__, "free_heap_size = %lu\n", esp_get_free_heap_size());
	
	const char* AP_WIFI_SSID = badge_obj.ap_ssid;
	const char* AP_WIFI_PASSWORD = badge_obj.ap_password;
	const char* STA_WIFI_SSID = badge_obj.sta_ssid;
	const char* STA_WIFI_PASSWORD = badge_obj.sta_password;

	wifi_config_t ap_config = { 0 };
	
	snprintf((char*)ap_config.ap.ssid, SIZEOF(ap_config.ap.ssid), "%s", AP_WIFI_SSID);
	snprintf((char*)ap_config.ap.password, SIZEOF(ap_config.ap.password), "%s", AP_WIFI_PASSWORD);
	ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
	ap_config.ap.ssid_len = strlen(AP_WIFI_SSID);
	ap_config.ap.max_connection = AP_MAX_STA_CONN;

	if (strlen(AP_WIFI_PASSWORD) == 0) {
		ap_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	wifi_config_t sta_config = { 0 };
	snprintf((char*)sta_config.sta.ssid, SIZEOF(sta_config.sta.ssid), "%s", STA_WIFI_SSID);
	snprintf((char*)sta_config.sta.password, SIZEOF(sta_config.sta.password), "%s", STA_WIFI_PASSWORD);


	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_APSTA) );
	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config) );
	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config) );
	ESP_ERROR_CHECK( esp_wifi_start() );
	ESP_LOGI(TAG, "WIFI_MODE_AP started. SSID:%s password:%s",
			 AP_WIFI_SSID, AP_WIFI_PASSWORD);

	ESP_ERROR_CHECK( esp_wifi_connect() );
	int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
								   pdFALSE, pdTRUE, STA_TIMEOUT_MS / portTICK_PERIOD_MS);
	ESP_LOGI(TAG, "bits=%x", bits);
	if (bits & CONNECTED_BIT) {
		ESP_LOGI(TAG, "WIFI_MODE_STA connected. SSID:%s password:%s",
			 STA_WIFI_SSID, STA_WIFI_PASSWORD);
        schedule_sync_handler(true);
	} else {
		ESP_LOGI(TAG, "WIFI_MODE_STA can't connected. SSID:%s password:%s",
			 STA_WIFI_SSID, STA_WIFI_PASSWORD);
		stop_wifi();
	}

    curr_mode = WIFI_MODE_APSTA;
	ESP_LOGI(__FILE__, "free_heap_size = %lu\n", esp_get_free_heap_size());

	return (bits & CONNECTED_BIT) != 0;
}

void stop_wifi(){
	ESP_LOGI(__FILE__, "free_heap_size = %lu\n", esp_get_free_heap_size());

    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_NULL);
    ESP_LOGI(TAG, "WIFI disabled");
	ESP_LOGI(__FILE__, "free_heap_size = %lu\n", esp_get_free_heap_size());
}

void wifi_task(void *arg)
{   
    wifi_queue = xQueueCreate(4, sizeof(uint32_t));
    uint32_t wifi_event;

    while (1) {
        if(!xQueueReceive(wifi_queue, &wifi_event, portMAX_DELAY))
            continue;
        
        switch (wifi_event) {
            case EVENT_HOTSPOT_START:
                stop_wifi();
                start_wifi_ap();
				esp_timer_start_once(inactivity_timer, AP_INACTIVITY_TIMEOUT_S * 1000000);
                break;
            case EVENT_STA_START:
                stop_wifi();
                start_wifi_sta();
				retry_num = 0;
                break;
            case EVENT_SYNC_START:
				schedule_sync_handler(true);
                break;
            case EVENT_HOTSPOT_STOP:
            case EVENT_STA_STOP:
                stop_wifi();
				esp_timer_stop(inactivity_timer);
                break;            
            default:
                ESP_LOGI(__FILE__, "not exists event 0x%04" PRIx32, wifi_event);
                break;
        }
        vTaskDelay( 1000 / portTICK_PERIOD_MS );
    }
}