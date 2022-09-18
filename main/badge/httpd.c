#include "httpd.h"

#define REST_TAG  __FILE__

static rest_server_context_t *rest_context = NULL;
static char* session_key = NULL;
static int client_count = 0;

static void reset_timer_callback(void* arg)
{
	esp_restart();
}

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "image/svg+xml";
    }
    return httpd_resp_set_type(req, type);
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    printf("free_heap_size = %d\n", esp_get_free_heap_size());
    rest_context = (rest_server_context_t *)req->user_ctx;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);
    printf("free_heap_size = %d\n", esp_get_free_heap_size());

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(REST_TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                printf("free_heap_size = %d\n", esp_get_free_heap_size());
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(REST_TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

#define is_string_match(a, c)    (!strncmp(a, c, sizeof(c)))

void byte_to_hex_str(char *xp, const char *bb, int n) 
{
    const char xx[]= "0123456789ABCDEF";
    while (--n >= 0) xp[n] = xx[(bb[n>>1] >> ((1 - (n&1)) << 2)) & 0xF];
}

static void session_destroy()
{
    ESP_LOGI(__FILE__, "Destroy Context function called");
    free(session_key);
    session_key = NULL;
}

static esp_err_t session_init(httpd_req_t *req) {
    char nonce[SESSION_KEY_LEN];
    esp_fill_random(nonce, SESSION_KEY_LEN);

    if (! session_key) {
        session_key = calloc(1, SESSION_KEY_LEN + 1); 
    }

    byte_to_hex_str(session_key, nonce, SESSION_KEY_LEN);

    return ESP_OK;
}

static bool check_session(httpd_req_t *req, const char* client_data) {
    if(!session_key) return false;
    if(!client_data) return false;

    cJSON *client_json = cJSON_Parse(client_data);
    cJSON *client_key = cJSON_GetObjectItem(client_json, "key");

    if(cJSON_IsString(client_key) && (client_key->valuestring != NULL)){
        ESP_LOGE(__FILE__, "Session key: %s", session_key);
        ESP_LOGE(__FILE__, "Client key: %s", client_key->valuestring);

        if(cJSON_IsString(client_key) && (client_key->valuestring != NULL)) {
            bool res = !strncmp(client_key->valuestring, session_key, SESSION_KEY_LEN);
            cJSON_Delete(client_json);
            return res;
        }
    }
    cJSON_Delete(client_json);
    return false;
}

static esp_err_t rest_send_response(httpd_req_t *req, char* response){
    ESP_LOGI(__FILE__, "free_heap_size = %d\n", esp_get_free_heap_size());
    if (httpd_resp_sendstr(req, response) != ESP_OK) {
        ESP_LOGE(__FILE__, "Response failed!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send response");
        ESP_LOGI(__FILE__, "free_heap_size = %d\n", esp_get_free_heap_size());
        return ESP_FAIL;
    }
    ESP_LOGI(__FILE__, "free_heap_size = %d\n", esp_get_free_heap_size());
    return ESP_OK;
}

static esp_err_t system_info_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    cJSON *response = cJSON_CreateObject();

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    cJSON_AddStringToObject(response, "IDF version", IDF_VER);
    cJSON_AddNumberToObject(response, "# cores", chip_info.cores);
    cJSON_AddStringToObject(response, "firmware branch", GIT_BRANCH);
    cJSON_AddStringToObject(response, "firmware commit", GIT_REV);
    cJSON_AddStringToObject(response, "firmware tag", GIT_TAG);

    char* response_str = cJSON_PrintUnformatted(response);
    
    esp_err_t err = rest_send_response(req, response_str);

    cJSON_free((void*)response_str);
    cJSON_Delete(response);
    return err;
}

static esp_err_t schedule_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "application/json");
    const char* buf = load_schedule_from_file();

    if(!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"result\": \"error\"}");
        return ESP_FAIL;
    }

    esp_err_t err = rest_send_response(req, (char*)buf);
    free((char*)buf);
    return err;
}

static esp_err_t login_handler(httpd_req_t *req, const char* client_data){
    httpd_resp_set_type(req, "application/json");

    cJSON *response = cJSON_CreateObject();
    cJSON *client_json = cJSON_Parse(client_data);
    
    cJSON *client_pass = cJSON_GetObjectItem(client_json, "password");
    char *badge_pass = badge_obj.web_login;
    esp_err_t err;
    if(cJSON_IsString(client_pass) && (client_pass->valuestring != NULL) 
        && !strncmp(client_pass->valuestring, badge_pass, strlen(badge_pass)))
    {
        session_init(req);

        cJSON_AddStringToObject(response, "key", session_key);
        char* response_str = cJSON_PrintUnformatted(response);

        err = rest_send_response(req, response_str);

        cJSON_free((void*)response_str);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"result\": \"error\"}");
        err = ESP_FAIL;
    }

    cJSON_Delete(response);
    cJSON_Delete(client_json);
    return err;
}

static esp_err_t logout_handler(httpd_req_t *req, const char* client_data){
    httpd_resp_set_type(req, "application/json");

    cJSON *response = cJSON_CreateObject();

    esp_err_t err;
    if(check_session(req, client_data)){
        session_destroy();
        
        char* response_str = cJSON_PrintUnformatted(response);
        
        err = rest_send_response(req, response_str);

        cJSON_free((void*)response_str);
    }
    else{
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"result\": \"error\"}");
        err = ESP_FAIL;
    }

    cJSON_Delete(response);
    
    return err;
}

static esp_err_t check_auth_handler(httpd_req_t *req, const char* client_data){
    httpd_resp_set_type(req, "application/json");

    cJSON *response = cJSON_CreateObject();

    esp_err_t err;
    if(check_session(req, client_data)){
        char* response_str = cJSON_PrintUnformatted(response);
        
        err = rest_send_response(req, response_str);

        cJSON_free((void*)response_str);
    }
    else{
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"result\": \"error\"}");
        err = ESP_FAIL;
    }

    cJSON_Delete(response);

    return err;
}

static esp_err_t radar_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "application/json");
    cJSON *response = cJSON_CreateObject();
    cJSON *radar = cJSON_AddArrayToObject(response, "radar");

    char buf[BADGE_BUF_SIZE] = {0};

    for (int i = 0; i < MAX_NEARBY_NODE; i++) {
        if(!ble_nodes[i].active) continue;

        cJSON *item = cJSON_CreateObject();
        
        //snprintf(buf, sizeof(buf), "%s", ble_nodes[i].name);
        cJSON_AddStringToObject(item, "name", ble_nodes[i].name);
        snprintf(buf, sizeof(buf), "%d dBm", ble_nodes[i].rssi);
        cJSON_AddStringToObject(item, "rssi", buf);
        snprintf(buf, sizeof(buf), "%d", ble_nodes[i].id);
        cJSON_AddStringToObject(item, "id", buf);
        cJSON_AddItemToArray(radar, item);
    }

    char* response_str = cJSON_PrintUnformatted(response);
    
    esp_err_t err = rest_send_response(req, response_str);
    cJSON_free((void*)response_str);
    cJSON_Delete(response);
    return err;
}

static esp_err_t badge_name_handler(httpd_req_t *req, const char* client_data){
    httpd_resp_set_type(req, "application/json");

    cJSON *response = cJSON_CreateObject();
    cJSON *client_json = cJSON_Parse(client_data);
   
    esp_err_t err;
    if(check_session(req, client_data)){
        cJSON* name = cJSON_GetObjectItem(client_json, "name");
        if(cJSON_IsString(name) && (name->valuestring != NULL)){
            if(strlen(name->valuestring) > 0){
                badge_obj.update(3, name->valuestring);
            }
        }
        cJSON_AddStringToObject(response, "name", badge_obj.device_name);
        char* response_str = cJSON_PrintUnformatted(response);
        
        err = rest_send_response(req, response_str);

        cJSON_free((void*)response_str);
    }
    else{
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"result\": \"error\"}");
        err = ESP_FAIL;
    }

    cJSON_Delete(response);
    cJSON_Delete(client_json);

    return err;
}

static esp_err_t wifi_handler(httpd_req_t *req, const char* client_data){
    httpd_resp_set_type(req, "application/json");

    cJSON *response = cJSON_CreateObject();
    cJSON *client_json = cJSON_Parse(client_data);
    
    esp_err_t err;
    if(check_session(req, client_data)) {
        cJSON* wifi = cJSON_GetObjectItem(client_json, "wifi");
        if(cJSON_IsObject(wifi)){
            cJSON* ssid = cJSON_GetObjectItem(wifi, "ssid");
            cJSON* password = cJSON_GetObjectItem(wifi, "password");

            if(cJSON_IsString(ssid) && (ssid->valuestring != NULL) && (strlen(ssid->valuestring) > 0)) {
                badge_obj.update(1, ssid->valuestring);
            } else if(cJSON_IsString(password) && (password->valuestring != NULL) && (strlen(password->valuestring) > 0)){
                badge_obj.update(2, password->valuestring);
            } 
        }
        
        cJSON *wifi_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(wifi_obj, "ssid", badge_obj.ap_ssid);
        cJSON_AddStringToObject(wifi_obj, "password", badge_obj.ap_password);
        cJSON_AddItemToObject(response, "wifi", wifi_obj);

        char* response_str = cJSON_PrintUnformatted(response);
        
        err = rest_send_response(req, response_str);

        cJSON_free((void*)response_str);
        
    }
    else{
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"result\": \"error\"}");
        err = ESP_FAIL;
    }

    cJSON_Delete(response);
    cJSON_Delete(client_json);

    return err;
}

static esp_err_t password_handler(httpd_req_t *req, const char* client_data){
    httpd_resp_set_type(req, "application/json");

    cJSON *response = cJSON_CreateObject();

    cJSON *client_json = cJSON_Parse(client_data);

    esp_err_t err;
    if(check_session(req, client_data)){
        cJSON* password = cJSON_GetObjectItem(client_json, "password");
        if(cJSON_IsString(password) && (password->valuestring != NULL) && (strlen(password->valuestring) > 0)) {
            badge_obj.update(0, password->valuestring);
        }
        char* response_str = cJSON_PrintUnformatted(response);
        
        err = rest_send_response(req, response_str);

        cJSON_free((void*)response_str);
    }
    else{
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"result\": \"error\"}");
        err = ESP_FAIL;
    }

    cJSON_Delete(response);
    cJSON_Delete(client_json);

    return err;
}

static esp_err_t reset_handler(httpd_req_t *req, const char* client_data){
    httpd_resp_set_type(req, "application/json");

    cJSON *response = cJSON_CreateObject();

    esp_err_t err;
    if(check_session(req, client_data)){
        char* response_str = cJSON_PrintUnformatted(response);

        err = rest_send_response(req, response_str);

        cJSON_free((void*)response_str);

        if(!unlink(SETTINGS_FILE)) {
            esp_timer_handle_t reset_timer;
            const esp_timer_create_args_t timer_args = {
                .callback = &reset_timer_callback,
                .name = "reset-timer"
            };

	        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &reset_timer));
            esp_timer_start_once(reset_timer, 3 * 1000000);
        }
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "{\"result\": \"error\"}");
        err = ESP_FAIL;
    }

    cJSON_Delete(response);

    return err;
}

static esp_err_t post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';
    
    char *cmd = strstr(req->uri, API_ENDPOINT);
    if (cmd != NULL)
        cmd += strlen(API_ENDPOINT);
    ESP_LOGI(__FILE__, "command: %s", cmd);
    
    if(is_string_match(cmd, "login")){
        login_handler(req, buf);
    } else if (is_string_match(cmd, "logout")) {
        logout_handler(req, buf);
    } else if (is_string_match(cmd, "check_authentication")) {
        check_auth_handler(req, buf);
    } else if (is_string_match(cmd, "schedule")) {
        schedule_handler(req);
    } else if (is_string_match(cmd, "info")) {
        system_info_handler(req);
    } else if (is_string_match(cmd, "radar")) {
        radar_handler(req);
    } else if (is_string_match(cmd, "name")) {
        badge_name_handler(req, buf);
    } else if (is_string_match(cmd, "wifi")) {
        wifi_handler(req, buf);
    } else if (is_string_match(cmd, "password")) {
        password_handler(req, buf);
    } else if (is_string_match(cmd, "reset")) {
        reset_handler(req, buf);
    } else {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "not found");
    }
    
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    // Start the httpd server
    ESP_LOGI(__FILE__, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        REST_CHECK(BASE_PATH, "wrong base path", err);
        rest_context = calloc(1, sizeof(rest_server_context_t));
        REST_CHECK(rest_context, "No memory for rest context", err);
        strlcpy(rest_context->base_path, BASE_PATH, sizeof(rest_context->base_path));

        // Registering the ws handler
        ESP_LOGI(__FILE__, "Registering URI handlers");
        /* URI handler for fetching system info */
        httpd_uri_t common_post_uri = {
            .uri = API_ENDPOINT_WILDCARD,
            .method = HTTP_POST,
            .handler = post_handler,
            .user_ctx = rest_context
        };
        httpd_register_uri_handler(server, &common_post_uri);

        /* URI handler for getting web server files */
        httpd_uri_t common_get_uri = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = get_handler,
            .user_ctx = rest_context
        };
        httpd_register_uri_handler(server, &common_get_uri);
        return server;
    }

err:
    ESP_LOGI(__FILE__, "Error starting server!");
    return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    free(rest_context);
    return httpd_stop(server);
}

void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    client_count++;
    ESP_LOGI(__FILE__, "Number of clients: %d", client_count);

    ESP_LOGI(__FILE__, "free_heap_size = %d\n", esp_get_free_heap_size());
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(__FILE__, "Starting webserver");
        *server = start_webserver();
    }
    ESP_LOGI(__FILE__, "free_heap_size = %d\n", esp_get_free_heap_size());
}

void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    client_count--;
    ESP_LOGI(__FILE__, "Number of clients: %d", client_count);

    ESP_LOGI(__FILE__, "free_heap_size = %d\n", esp_get_free_heap_size());
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server && !client_count) {
        ESP_LOGI(__FILE__, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(__FILE__, "Failed to stop http server");
        }
    }
    ESP_LOGI(__FILE__, "free_heap_size = %d\n", esp_get_free_heap_size());
}



