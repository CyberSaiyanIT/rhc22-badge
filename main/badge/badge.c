#include <stdio.h>
#include <string.h>

#include "badge.h"

ble_node_t ble_nodes[MAX_NEARBY_NODE];
badge_obj_t badge_obj;

uint8_t count_ble_nodes(){
    uint8_t cnt = 0;
    for(int i=0; i<MAX_NEARBY_NODE; i++)
    {
        if(ble_nodes[i].active) cnt++;
    }
    return cnt;
}

bool check_ble_set()
{   
    uint8_t set_bits = 0;
    set_bits |= 1 << (badge_obj.device_id-1);

    for(int i=0; i<MAX_NEARBY_NODE; i++)
    {
        if(ble_nodes[i].active){
            set_bits |= 1 << (ble_nodes[i].id-1);
        }
    }
    return set_bits == 0x7F;
}

char* load_file_content(char* filename){
    struct stat file_stat;
    if (stat(filename, &file_stat) == -1) {
        ESP_LOGI(__FILE__, "File not found");
        return NULL;
    }

    char *content_buf = (char*) calloc(1, file_stat.st_size + 1);
    if(!content_buf) {
        ESP_LOGI(__FILE__, "Cannot allocate memory");
    }
    FILE *fp = fopen(filename, "r");
    if(!fp) {
        ESP_LOGI(__FILE__, "Cannot open file");
    }
    fread(content_buf, 1, file_stat.st_size, fp);
    fclose(fp);

    return content_buf;
}

char* load_schedule_from_file() {
    FILE *fp = fopen(SCHEDULE_FILE, "r");
    
    if(!fp){
        ESP_LOGE(__FILE__, "Cannot open file %s", SCHEDULE_FILE);
        return NULL;
    }

    /* Get the file size */
    fseek(fp, 0, SEEK_END); // seek to end of file
    int file_size = ftell(fp);  // get current file pointer
    fseek(fp, 0, SEEK_SET); // seek back to beginning of file

    char* data_buf = (char*)calloc(1, file_size + 1);
    if(!data_buf){
        fclose(fp);
        ESP_LOGE(__FILE__, "Cannot allocate memory of %d bytes", file_size);
        return NULL;
    } 
    fread(data_buf, 1, file_size, fp);
    fclose(fp);
    return data_buf;
}

cJSON* load_default() {
    char* default_content = load_file_content(DEFAULT_FILE);
    return cJSON_Parse(default_content);
}

cJSON* load_settings() {
    char* default_content = load_file_content(SETTINGS_FILE);
    return cJSON_Parse(default_content);
}

const char* json_get_str_value(cJSON* obj, const char *key)
{
    char* value = cJSON_GetObjectItem(obj, key)->valuestring;
    ESP_LOGI(__FILE__, "Get %s value %s", key, value);
    return value;
}

int json_get_int_value(cJSON* obj, const char *key)
{
    int value = cJSON_GetObjectItem(obj, key)->valueint;
    ESP_LOGI(__FILE__, "Get %s value %d", key, value);
    return value;
}

void json_set_str_value(cJSON* obj, const char *key, const char *value)
{
    if(cJSON_GetObjectItem(obj, key)){
        cJSON_DeleteItemFromObject(obj, key);
    }
    
    cJSON_AddStringToObject(obj, key, value);
    ESP_LOGI(__FILE__, "Set %s value %s", key, value);
}

void json_set_int_value(cJSON* obj, const char *key, const double value)
{
    if(cJSON_GetObjectItem(obj, key)){
        cJSON_DeleteItemFromObject(obj, key);
    }

    cJSON_AddNumberToObject(obj, key, value);
    ESP_LOGI(__FILE__, "Set %s value %f", key, value);
}

void save_settings(cJSON* json_settings) {
    // Save to settings.json
    char *json = cJSON_Print(json_settings);
    FILE *fp = fopen(SETTINGS_FILE, "w");
    fprintf(fp, "%s", json);
    cJSON_free(json);
    fclose(fp);
    
    ESP_LOGI(__FILE__, "Setting file saved");
}

bool update_attribute(int id, char* data) { 
    switch(id){
        case 0: // Web login password
            snprintf(badge_obj.web_login, SIZEOF(badge_obj.web_login), "%s", data);
            break;
        case 1: // WiFi SSID
            snprintf(badge_obj.ap_ssid, SIZEOF(badge_obj.ap_ssid), "%s", data);
            break;
        case 2: // WiFi Password
            snprintf(badge_obj.ap_password, SIZEOF(badge_obj.ap_password), "%s", data);
            break;
        case 3: // Device name
            snprintf(badge_obj.device_name, SIZEOF(badge_obj.device_name), "%s", data);
            break;
    }
    
    cJSON* json_settings = load_settings();
    cJSON *obj_badge = cJSON_GetObjectItem(json_settings, "badge");
    cJSON *obj_web = cJSON_GetObjectItem(json_settings, "web");
    cJSON *obj_ap = cJSON_GetObjectItem(json_settings, "ap");

    json_set_str_value(obj_badge, "name", badge_obj.device_name);
    json_set_str_value(obj_ap, "ssid", badge_obj.ap_ssid);
    json_set_str_value(obj_ap, "password", badge_obj.ap_password);
    json_set_str_value(obj_web, "login", badge_obj.web_login);

    save_settings(json_settings);
    cJSON_Delete(json_settings);
    return true;
}

uint8_t generate_id(uint16_t seed) {
    //srand(seed);
    return 1+(esp_random()%7);
}

void badge_init(){
    // Init storage
    nvs_init();
    spiffs_init();

    // Init event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Init settings
    struct stat file_stat;

    cJSON* json_settings = NULL;

    if (stat(SETTINGS_FILE, &file_stat) == -1) {
        json_settings = load_default();

        cJSON *obj_badge = cJSON_GetObjectItem(json_settings, "badge");
        cJSON *obj_web = cJSON_GetObjectItem(json_settings, "web");
        cJSON *obj_ap = cJSON_GetObjectItem(json_settings, "ap");
        cJSON *obj_sta = cJSON_GetObjectItem(json_settings, "sta");

        const char* web_login = json_get_str_value(obj_web, "login");
        const char* sta_ssid = json_get_str_value(obj_sta, "ssid");
        const char* sta_password = json_get_str_value(obj_sta, "password");

        // Update name and AP settings
        esp_efuse_mac_get_default(badge_obj.mac);
        badge_obj.short_mac = (badge_obj.mac[4] << 8) + badge_obj.mac[5];
        badge_obj.device_id = generate_id(badge_obj.short_mac);//1 + (badge_obj.short_mac % 7);
        snprintf(badge_obj.device_name, SIZEOF(badge_obj.device_name), "Saiyan-%04x", badge_obj.short_mac);
        snprintf(badge_obj.ap_ssid, SIZEOF(badge_obj.ap_ssid), "Saiyan-%04x", badge_obj.short_mac);
        snprintf(badge_obj.ap_password, SIZEOF(badge_obj.ap_password), "%02x%02x%02x%02x", badge_obj.mac[2], badge_obj.mac[3], badge_obj.mac[4], badge_obj.mac[5]);

        snprintf(badge_obj.web_login, SIZEOF(badge_obj.web_login), "%s", web_login);
        snprintf(badge_obj.sta_ssid, SIZEOF(badge_obj.sta_ssid), "%s", sta_ssid);
        snprintf(badge_obj.sta_password, SIZEOF(badge_obj.sta_password), "%s", sta_password);

        // Update settings object
        cJSON_AddNumberToObject(obj_badge, "id", (double)badge_obj.device_id);
        json_set_str_value(obj_badge, "name", badge_obj.device_name);
        json_set_str_value(obj_ap, "ssid", badge_obj.ap_ssid);
        json_set_str_value(obj_ap, "password", badge_obj.ap_password);

        // Save to settings.json
        save_settings(json_settings);
        
        ESP_LOGI(__FILE__, "Setting file saved to default values");
    }
    
    json_settings = load_settings();

    cJSON *obj_badge = cJSON_GetObjectItem(json_settings, "badge");
    cJSON *obj_web = cJSON_GetObjectItem(json_settings, "web");
    cJSON *obj_ap = cJSON_GetObjectItem(json_settings, "ap");
    cJSON *obj_sta = cJSON_GetObjectItem(json_settings, "sta");

    const char* badge_name = json_get_str_value(obj_badge, "name"); // should be empty
    const char* web_login = json_get_str_value(obj_web, "login");
    const char* ap_ssid = json_get_str_value(obj_ap, "ssid"); // should be empty
    const char* ap_password = json_get_str_value(obj_ap, "password"); // should be empty
    const char* sta_ssid = json_get_str_value(obj_sta, "ssid");
    const char* sta_password = json_get_str_value(obj_sta, "password");

    // Update badge object
    esp_efuse_mac_get_default(badge_obj.mac);
    
    badge_obj.short_mac = (badge_obj.mac[4] << 8) + badge_obj.mac[5];
    badge_obj.device_id = (int)cJSON_GetObjectItem(obj_badge, "id")->valuedouble;
    //generate_id(badge_obj.short_mac);//1 + (badge_obj.short_mac % 7);

    snprintf(badge_obj.device_name, SIZEOF(badge_obj.device_name), "%s", badge_name);
    snprintf(badge_obj.web_login, SIZEOF(badge_obj.web_login), "%s", web_login);
    snprintf(badge_obj.ap_ssid, SIZEOF(badge_obj.ap_ssid), "%s", ap_ssid);
    snprintf(badge_obj.ap_password, SIZEOF(badge_obj.ap_password), "%s", ap_password);
    snprintf(badge_obj.sta_ssid, SIZEOF(badge_obj.sta_ssid), "%s", sta_ssid);
    snprintf(badge_obj.sta_password, SIZEOF(badge_obj.sta_password), "%s", sta_password);
    
    ESP_LOGI(__FILE__, "Setting file loaded");

    cJSON_Delete(json_settings);
    badge_obj.update = update_attribute;
}
