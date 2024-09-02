#include "ui.h"

enum screen_order {SCREEN_LOGO, SCREEN_EVENT, SCREEN_RADAR, SCREEN_RSSI,  SCREEN_ADMIN, SCREEN_SNAKE, NUM_SCREENS};
static lv_obj_t* screens[NUM_SCREENS];
static int8_t current_screen = SCREEN_LOGO;

static uint32_t last_trigger = -1;

static lv_obj_t *radar_node[MAX_NEARBY_NODE] = {0};
static lv_obj_t *radar_node_number[MAX_NEARBY_NODE] = {0};
static lv_obj_t *table_rssi, *table_event;
static lv_obj_t *admin_switch, *admin_switch_sta, *admin_sync;
static lv_obj_t *admin_switch_text, *admin_switch_sta_text, *admin_sync_text;
static lv_obj_t *admin_ssid, *admin_pass;

static bool ap_started = false;
static bool sta_connected = false;

static uint8_t admin_state = ADMIN_STATE_OFF;

void restore_current_task(){
    if(current_screen == SCREEN_RSSI){
        lv_task_set_prio(rssi_task_handle, LV_TASK_PRIO_LOW);
    }
    else if(current_screen == SCREEN_RADAR){
        lv_task_set_prio(radar_task_handle, LV_TASK_PRIO_LOW);
    }
}

void pause_current_task(){
    if(current_screen == SCREEN_RSSI){
        lv_task_set_prio(rssi_task_handle, LV_TASK_PRIO_OFF);
    }
    else if(current_screen == SCREEN_RADAR){
        lv_task_set_prio(radar_task_handle, LV_TASK_PRIO_OFF);
    }
}

/*
* Update the screen backlight status
* returns status of backlight (true if backlight off)
*/

static bool ui_update_backlight(bool trigger)
{
    uint32_t span = lv_tick_get() - last_trigger;

    if (trigger)
    {
        set_screen_led_backlight(SCREEN_BRIGHT_MAX);
        last_trigger = lv_tick_get();

        restore_current_task();
    }
    else
    {
        if (span > BRIGHT_OFF_TIMEOUT_MS){
            set_screen_led_backlight(SCREEN_BRIGHT_OFF);
            pause_current_task();
        }
        else if (span > BRIGHT_MID_TIMEOUT_MS){
            set_screen_led_backlight(SCREEN_BRIGHT_MID);
        }
    }

    /* Avoid doing action when backlight off */
    if (span > BRIGHT_OFF_TIMEOUT_MS){
        return true;
    }
    return false;
}

void ui_send_wifi_event(int event)
{
    xQueueSend(wifi_queue, &event, portMAX_DELAY);
}

void scroll_up(lv_obj_t *screen){
    lv_obj_t *page = lv_obj_get_child(screen, NULL);
    lv_page_scroll_ver(page, 80);
}

void scroll_down(lv_obj_t *screen){
    lv_obj_t *page = lv_obj_get_child(screen, NULL);
    lv_page_scroll_ver(page, -80);
}

void ui_button_up()
{
    if(ui_update_backlight(true)){
        return;
    }

    switch(current_screen){
        case SCREEN_SNAKE:
            lv_task_set_prio(snake_task_handle, LV_TASK_PRIO_LOW);
            snake_set_dir(1);
            break;
        case SCREEN_ADMIN:
            switch(admin_state){
                case ADMIN_STATE_OFF: // AP and STA disabled: enable AP
                    ui_send_wifi_event(EVENT_HOTSPOT_START);
                    lv_obj_set_hidden(admin_switch_sta, true);
                    admin_state = ADMIN_STATE_AP;
                    break;
                case ADMIN_STATE_AP: // AP enabled: disable AP
                    ui_send_wifi_event(EVENT_HOTSPOT_STOP);
                    lv_obj_set_hidden(admin_switch_sta, false);
                    lv_obj_set_hidden(admin_ssid, true);
                    admin_state = ADMIN_STATE_OFF;
                    break;
            }   
            break;
        case SCREEN_EVENT:
        case SCREEN_RSSI:
            scroll_up(screens[current_screen]);
            break;
        default:
            ESP_LOGI(__FILE__, "Button up, no actions");
    }
}

void ui_button_down()
{
    if(ui_update_backlight(true)){
        return;
    }

    switch(current_screen){
        case SCREEN_SNAKE:
            lv_task_set_prio(snake_task_handle, LV_TASK_PRIO_LOW);
            snake_set_dir(-1);
            break;
        case SCREEN_ADMIN:
            switch(admin_state){
                case ADMIN_STATE_OFF: // AP and STA disabled: enable STA
                    ui_send_wifi_event(EVENT_STA_START);
                    lv_label_set_text(admin_switch_sta_text, "Started...");
                    admin_state = ADMIN_STATE_STA;
                    break;
            }
            break;
        case SCREEN_EVENT:
        case SCREEN_RSSI:
            scroll_down(screens[current_screen]);
            break;
        default:
            ESP_LOGI(__FILE__, "Button down, no actions");
    }
}

void ui_event_load()
{
    const char* buf = load_schedule_from_file();
    if (buf == NULL)
    {
        ESP_LOGI(__FILE__, "Failed to load schedule from file");
        return;
    }
    cJSON* schedule_json = cJSON_Parse(buf);
    free((char*)buf);

    cJSON* schedule_array = cJSON_GetObjectItem(schedule_json, "schedule");

    int size = cJSON_GetArraySize(schedule_array);
    lv_table_set_row_cnt(table_event, size);
    //lv_table_set_col_cnt(table_event, 2);

    char buff[256];
    for (int i = 0; i < size; i++)
    {
        cJSON* item = cJSON_GetArrayItem(schedule_array, i);

        //cJSON *index = cJSON_GetObjectItem(item, "index");
        cJSON *title = cJSON_GetObjectItem(item, "title");
        cJSON *day = cJSON_GetObjectItem(item, "day");
        cJSON *hour = cJSON_GetObjectItem(item, "hour");
        cJSON *speaker = cJSON_GetObjectItem(item, "speaker");
        cJSON *location = cJSON_GetObjectItem(item, "location");
        cJSON *duration = cJSON_GetObjectItem(item, "duration");

        //ESP_LOGI(__FILE__, "Index %d: %s", i, speaker->valuestring);

        snprintf(buff, sizeof(buff), "DAY %s (%s)\n@ %s\n~%s",
                 day->valuestring, hour->valuestring, 
                 location->valuestring, duration->valuestring);
        lv_table_set_cell_value(table_event, i, 0, buff);

        snprintf(buff, sizeof(buff), "%s\nby %s",
            title->valuestring, speaker->valuestring);
        lv_table_set_cell_value(table_event, i, 1, buff);
    }

    cJSON_Delete(schedule_json);
}

static void ui_rssi_task(lv_task_t *arg)
{
    if (lv_scr_act() != screen_rssi)
    {
        lv_task_set_prio(rssi_task_handle, LV_TASK_PRIO_OFF);
        return;
    }
    
    const uint8_t count = count_ble_nodes();
    lv_table_set_row_cnt(table_rssi, count+1);

    char buf[BADGE_BUF_SIZE] = {0};

    uint8_t pos = 1;

    for (int i = 0; i < MAX_NEARBY_NODE; i++) {
        if(!ble_nodes[i].active) continue;
        
        lv_table_set_cell_value(table_rssi, pos, 0, ble_nodes[i].name);
        lv_table_set_cell_align(table_rssi, pos, 0, LV_LABEL_ALIGN_CENTER);

        snprintf(buf, sizeof(buf), "%d dBm", ble_nodes[i].rssi);
        lv_table_set_cell_value(table_rssi, pos, 1, buf);
        lv_table_set_cell_align(table_rssi, pos, 1, LV_LABEL_ALIGN_CENTER);

        snprintf(buf, sizeof(buf), "0x0%d", ble_nodes[i].id);
        lv_table_set_cell_value(table_rssi, pos, 2, buf);
        lv_table_set_cell_align(table_rssi, pos, 2, LV_LABEL_ALIGN_CENTER);

        pos++;
    }
}

static void ui_backlight_task(lv_task_t *arg){
    ui_update_backlight(false);
}

static void ui_radar_task(lv_task_t *arg)
{

    if (lv_scr_act() != screen_radar)
    {
        lv_task_set_prio(radar_task_handle, LV_TASK_PRIO_OFF);
        return;
    }

    if (lv_scr_act() == screen_radar)
    {
        static int mode;
        if (mode)
        {
            for (int i = 0; i < MAX_NEARBY_NODE; i++)
            {
                if(ble_nodes[i].active) {
                    lv_coord_t x = rand() % 80, y = rand() % 60;

                    if (ble_nodes[i].rssi > -70)
                    {
                        // near range.
                        if (x < 50)
                            x = (LV_HOR_RES - x) / 2 - 20;
                        else
                            x = (LV_HOR_RES + x) / 2 - 20;
                        if (y < 40)
                            y = (LV_VER_RES - y) / 2 - 20;
                        else
                            y = (LV_VER_RES + y) / 2 - 20;
                    }
                    else if (ble_nodes[i].rssi > -90)
                    {
                        // middle range.
                        if (x < 50)
                            x = (LV_HOR_RES - 100 - x) / 2;
                        else
                            x = (LV_HOR_RES + 100 + x) / 2 - 20;
                        if (y < 40)
                            y = (LV_VER_RES - 80 - y) / 2;
                        else
                            y = (LV_VER_RES + 80 - y) / 2 - 20;
                    }
                    else
                    {
                        // long range
                        if (x < 50)
                            x = (LV_HOR_RES - 200 - x) / 2;
                        else
                            x = (LV_HOR_RES + 200 + x) / 2 - 20;
                        if (y < 40)
                            y = (LV_VER_RES - 160 - y) / 2;
                        else
                            y = (LV_VER_RES + 160 + y) / 2 - 20;
                    }

                    lv_obj_set_pos(radar_node[i], x, y);
                    lv_label_set_text_fmt(radar_node_number[i], "%d", ble_nodes[i].id);
                    lv_obj_set_hidden(radar_node[i], false);
                    lv_obj_fade_in(radar_node[i], 1000, 0);
                } else {
                    lv_obj_set_hidden(radar_node[i], true);
                }
            }
        }
        else
        {
            for (int i = 0; i < MAX_NEARBY_NODE; i++)
            {
                if (lv_obj_is_visible(radar_node[i]))
                    lv_obj_fade_out(radar_node[i], 1000, 0);
            }
        }

        mode = !mode;
    }
}

void ui_screen_event_init() {
    // page for event
    static lv_style_t style;
    lv_style_init(&style);

    screen_event = lv_obj_create(NULL, NULL);
    lv_obj_t *screen_event_page = lv_page_create(screen_event, NULL);

    lv_obj_t *scrollable = lv_page_get_scrollable(screen_event_page);
    lv_cont_set_layout(scrollable, LV_LAYOUT_PRETTY_TOP);
    lv_obj_set_style_local_pad_all(scrollable, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_inner(scrollable, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_all(screen_event_page, LV_PAGE_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_inner(screen_event_page, LV_PAGE_PART_BG, LV_STATE_DEFAULT, 0);
    lv_page_set_scrollbar_mode(screen_event_page, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_size(screen_event_page, LV_HOR_RES, LV_VER_RES);
    lv_coord_t content_w = lv_obj_get_width_grid(screen_event_page, 2, 1);

    lv_style_set_text_font(&style, LV_OBJ_PART_MAIN, &lv_font_montserrat_10);

    table_event = lv_table_create(screen_event_page, NULL);
    lv_obj_clean_style_list(table_event, LV_TABLE_PART_BG);
    lv_obj_set_drag_parent(table_event, true);
    lv_table_set_col_cnt(table_event, 2);
    lv_table_set_col_width(table_event, 0, 4*(2 * content_w)/10);
    lv_table_set_col_width(table_event, 1, 6*(2 * content_w)/10);
    lv_obj_add_style(table_event, LV_OBJ_PART_MAIN, &style);

    lv_obj_align(table_event, screen_event_page, LV_ALIGN_OUT_TOP_LEFT, 0, 0);

    ui_event_load(); 

    screens[SCREEN_EVENT] = screen_event;
}

void ui_screen_splash_init(){
    LV_IMG_DECLARE(img_logo);

    screen_logo = lv_obj_create(NULL, NULL);
    lv_obj_t *logo = lv_img_create(screen_logo, NULL);
    lv_img_set_src(logo, &img_logo);
    lv_obj_align(logo, NULL, LV_ALIGN_CENTER, 0, 0);
  /*Change the logo's background color*/
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_opa(&style, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_style_set_bg_color(&style, LV_STATE_DEFAULT, LV_COLOR_MAKE(0x34, 0x3a, 0x40));
    lv_obj_add_style(logo, LV_OBJ_PART_MAIN, &style);

    screens[SCREEN_LOGO] = screen_logo;
}

void ui_screen_radar_init(){
    // Page for radar
    LV_IMG_DECLARE(img_radar);
    
    screen_radar = lv_obj_create(NULL, NULL);
    lv_obj_t *img = lv_img_create(screen_radar, NULL);
    lv_img_set_src(img, &img_radar);
    lv_obj_align(img, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

    for (int i = 0; i < sizeof(radar_node) / sizeof(lv_obj_t *); i++)
    {
        radar_node[i] = lv_btn_create(img, NULL);
        lv_obj_set_size(radar_node[i], 20, 20);
        lv_btn_toggle(radar_node[i]); // set to solid color.
        lv_obj_set_hidden(radar_node[i], true);
        radar_node_number[i] = lv_label_create(radar_node[i], NULL);
        lv_label_set_text(radar_node_number[i], "X");
    }

    screens[SCREEN_RADAR] = screen_radar;
}

void ui_screen_rssi_init(){
    // page for rssi
    static lv_style_t style;
    lv_style_init(&style);

    screen_rssi = lv_obj_create(NULL, NULL);
    lv_obj_t *screen_rssi_page = lv_page_create(screen_rssi, NULL);

    lv_obj_t *scrollable = lv_page_get_scrollable(screen_rssi_page);
    lv_cont_set_layout(scrollable, LV_LAYOUT_PRETTY_TOP);
    lv_obj_set_style_local_pad_all(scrollable, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_pad_inner(scrollable, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_all(screen_rssi_page, LV_PAGE_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_inner(screen_rssi_page, LV_PAGE_PART_BG, LV_STATE_DEFAULT, 0);
    lv_page_set_scrollbar_mode(screen_rssi_page, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_size(screen_rssi_page, LV_HOR_RES, LV_VER_RES);
    lv_coord_t content_w = lv_obj_get_width_grid(screen_rssi_page, 3, 1);

    lv_style_set_text_font(&style, LV_OBJ_PART_MAIN, &lv_font_montserrat_12);

    table_rssi = lv_table_create(screen_rssi_page, NULL);
    lv_obj_clean_style_list(table_rssi, LV_TABLE_PART_BG);
    lv_obj_set_drag_parent(table_rssi, true);
    lv_table_set_col_cnt(table_rssi, 3);
    lv_table_set_col_width(table_rssi, 0, content_w);
    lv_table_set_col_width(table_rssi, 1, content_w);
    lv_table_set_col_width(table_rssi, 2, content_w);
    lv_obj_add_style(table_rssi, LV_OBJ_PART_MAIN, &style);

    lv_obj_align(table_rssi, screen_rssi_page, LV_ALIGN_OUT_TOP_LEFT, 0, 0);

    lv_table_set_cell_value(table_rssi, 0, 0, "NAME");
    lv_table_set_cell_value(table_rssi, 0, 1, "RSSI");
    lv_table_set_cell_value(table_rssi, 0, 2, "ID");

    lv_table_set_cell_align(table_rssi, 0, 0, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(table_rssi, 0, 1, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(table_rssi, 0, 2, LV_LABEL_ALIGN_CENTER);

    screens[SCREEN_RSSI] = screen_rssi;
}

void ui_screen_admin_init(){
    // page for admin
    screen_admin = lv_obj_create(NULL, NULL);
    admin_switch = lv_btn_create(screen_admin, NULL);
    lv_obj_set_size(admin_switch, 200, 50);
    lv_obj_set_pos(admin_switch, 60, 35);
    admin_switch_text = lv_label_create(admin_switch, NULL);
    lv_label_set_text(admin_switch_text, "TURN ON AP");

    admin_sync = lv_btn_create(screen_admin, NULL);
    lv_obj_set_size(admin_sync, 200, 50);
    lv_obj_set_pos(admin_sync, 60, 160);
    admin_sync_text = lv_label_create(admin_sync, NULL);
    lv_label_set_text(admin_sync_text, "FORCE SCHEDULE SYNC");
    lv_obj_set_hidden(admin_sync, true);

    admin_ssid = lv_label_create(screen_admin, NULL);
    lv_obj_align(admin_ssid, admin_switch, LV_ALIGN_OUT_BOTTOM_MID, -50, 10);
    lv_obj_set_hidden(admin_ssid, true);
    admin_pass = lv_label_create(screen_admin, NULL);
    lv_obj_align(admin_pass, admin_switch, LV_ALIGN_OUT_BOTTOM_MID, -50, 30);
    lv_obj_set_hidden(admin_pass, true);

    admin_switch_sta = lv_btn_create(screen_admin, NULL);
    lv_obj_set_size(admin_switch_sta, 200, 50);
    lv_obj_set_pos(admin_switch_sta, 60, 160);
    admin_switch_sta_text = lv_label_create(admin_switch_sta, NULL);
    lv_label_set_text(admin_switch_sta_text, "SYNC SCHEDULE");

    screens[SCREEN_ADMIN] = screen_admin;
}

void ui_screen_snake_init(){
    // page for snake
    screen_snake = lv_obj_create(NULL, NULL);
    snake_reset(screen_snake);

    screens[SCREEN_SNAKE] = screen_snake;
}

void ui_ap_start_handler() {
    ap_started = true;

    lv_label_set_text(admin_switch_text, "TURN OFF AP");

    char buf[BADGE_BUF_SIZE + 19] = {0};
    snprintf(buf, sizeof(buf), "SSID: %s", badge_obj.ap_ssid);
    lv_label_set_text(admin_ssid, buf);
    snprintf(buf, sizeof(buf), "PASS: %s", badge_obj.ap_password);
    lv_label_set_text(admin_pass, buf);

    lv_obj_set_hidden(admin_ssid, false);
    lv_obj_set_hidden(admin_pass, false);

    lv_btn_set_state(admin_switch, LV_BTN_STATE_CHECKED_PRESSED);
    admin_state = ADMIN_STATE_AP;
}

void ui_ap_stop_handler(){
    ap_started = false;

    lv_label_set_text(admin_switch_text, "TURN ON AP");
    lv_obj_set_hidden(admin_ssid, true);
    lv_obj_set_hidden(admin_pass, true);
    lv_obj_set_hidden(admin_switch_sta, false);

    lv_btn_set_state(admin_switch, LV_BTN_STATE_RELEASED);//enabl(admin_switch);
    admin_state = ADMIN_STATE_OFF;
}

void ui_sta_connected_handler(){
    sta_connected = true;

    lv_btn_set_state(admin_switch_sta, LV_BTN_STATE_CHECKED_PRESSED);
    lv_label_set_text(admin_switch_sta_text, "Downloading...");
    admin_state = ADMIN_STATE_STA;
}

void ui_sta_disconnected_handler(){
    sta_connected = false;
    lv_btn_set_state(admin_switch_sta, LV_BTN_STATE_RELEASED);
    admin_state = ADMIN_STATE_OFF;
}

void ui_sta_stop_handler() {
    sta_connected = false;
    lv_label_set_text(admin_switch_sta_text, "SYNC SCHEDULE");
    admin_state = ADMIN_STATE_OFF;
}

void ui_connection_progress(uint8_t cur, uint8_t max){
    if(cur != max){
        char buf[BADGE_BUF_SIZE + 20] = {0}; // Increase the size of buf to accommodate the entire formatted string
        snprintf(buf, sizeof(buf), "Connecting (%d/%d)", cur, max);
        lv_label_set_text(admin_switch_sta_text, buf);
        lv_obj_set_hidden(admin_switch_sta_text, false);
    } else {
        lv_label_set_text(admin_switch_sta_text, "Connection failed!");
        lv_obj_set_hidden(admin_switch_sta_text, false);
    }
}

void ui_toggle_sync(){
    lv_btn_set_state(admin_sync, LV_BTN_STATE_RELEASED);
    ui_send_wifi_event(EVENT_STA_STOP);
}

static void ui_init(void)
{
    ui_screen_splash_init();

    ui_screen_event_init();

    ui_screen_radar_init();
    
    ui_screen_rssi_init();

    ui_screen_admin_init();

    ui_screen_snake_init();
    
    radar_task_handle = lv_task_create(ui_radar_task, 2000, LV_TASK_PRIO_OFF, NULL);
    rssi_task_handle = lv_task_create(ui_rssi_task, 2000, LV_TASK_PRIO_OFF, NULL);
    snake_task_handle = lv_task_create(snake_task, 50, LV_TASK_PRIO_OFF, NULL);

    // show first page.
    lv_scr_load(screens[current_screen]);

    // Turn on backlight and run backlight management task
    ui_update_backlight(true);
    backlight_task_handle = lv_task_create(ui_backlight_task, 1000, LV_TASK_PRIO_LOW, NULL);

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, &ui_ap_start_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STOP, &ui_ap_stop_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ui_sta_connected_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &ui_sta_disconnected_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_STOP, &ui_sta_stop_handler, NULL));
}

static void ui_tick_task(void *arg)
{
    lv_tick_inc(1);
}

void ui_task(void *arg)
{
    SemaphoreHandle_t xGuiSemaphore;
    xGuiSemaphore = xSemaphoreCreateMutex();

    lv_init();
    lvgl_driver_init();

    lv_color_t *buf1 = (lv_color_t*) heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_color_t *buf2 = (lv_color_t*) heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);

    static lv_disp_buf_t disp_buf;
    uint32_t size_in_px = DISP_BUF_SIZE;

    lv_disp_buf_init(&disp_buf, buf1, buf2, size_in_px);
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &ui_tick_task,
        .name = "ui_tick_task",
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000));

    ui_init();

    while (1)
    {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
        {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    free(buf1);
    free(buf2);
    vTaskDelete(NULL);
}

void ui_switch_page_down()
{
    ui_update_backlight(true);

    current_screen++;
    current_screen %= NUM_SCREENS;
    ESP_LOGI("DISPLAY", "DISPLAY COUNTER: %d/%d", current_screen+1, NUM_SCREENS);

    lv_scr_load_anim(screens[current_screen], LV_SCR_LOAD_ANIM_OVER_TOP, 300, 0, false);

    restore_current_task();
}

void ui_switch_page_up()
{
    ui_update_backlight(true);

    current_screen--;
    current_screen = (NUM_SCREENS + (current_screen % NUM_SCREENS)) % NUM_SCREENS;
    ESP_LOGI("DISPLAY", "DISPLAY COUNTER: %d/%d", current_screen+1, NUM_SCREENS);
    
    lv_scr_load_anim(screens[current_screen], LV_SCR_LOAD_ANIM_OVER_BOTTOM, 300, 0, false);

    restore_current_task();
}

void button_task(void *arg)
{
    static button_event_t curr_ev;
    static button_event_t prev_ev[2];
    static QueueHandle_t button_events;
    button_events = button_init(PIN_BIT(BUTTON_1) | PIN_BIT(BUTTON_2));

    while (true)
    {
        if (xQueueReceive(button_events, &curr_ev, 1000 / portTICK_PERIOD_MS))
        {
            uint8_t btn_id = curr_ev.pin - 0x08;
            if (curr_ev.event == BUTTON_HELD)
            {
                set_screen_led_backlight(SCREEN_BRIGHT_MID);
            }
            if (curr_ev.pin == BUTTON_1) // DOWN button event
            {
                if ((prev_ev[btn_id].event == BUTTON_HELD) && (curr_ev.event == BUTTON_UP))
                {
                    ui_switch_page_down();
                }
                else if ((prev_ev[btn_id].event == BUTTON_DOWN) && (curr_ev.event == BUTTON_UP))
                {
                    ui_button_down();
                }
            }

            if (curr_ev.pin == BUTTON_2) // UP button event
            {
                if ((prev_ev[btn_id].event == BUTTON_HELD) && (curr_ev.event == BUTTON_UP))
                {
                    ui_switch_page_up();
                }
                else if ((prev_ev[btn_id].event == BUTTON_DOWN) && (curr_ev.event == BUTTON_UP))
                {
                    ui_button_up();
                }
            }
            prev_ev[btn_id] = curr_ev;
        }
    }
}
