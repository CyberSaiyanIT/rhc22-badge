#include "bt.h"

typedef struct {
    char scan_local_name[32];
    uint8_t name_len;
} ble_scan_local_name_t;

typedef struct {
    uint8_t *q_data;
    uint16_t q_data_len;
} host_rcv_data_t;

static uint8_t hci_cmd_buf[128];

static uint16_t scanned_count = 0;
static QueueHandle_t adv_queue;

static void init_ble_nodes() {
    for(int i=0; i<MAX_NEARBY_NODE; i++) {
        ble_nodes[i].name[0] = '\0';
        ble_nodes[i].active = false;
    }
}

static int check_duplicate(const ble_node_t* item) {
    // ESP_LOGI(__FILE__, "STRCMP result: %d", strcmp(ble_nodes[0].name, item->name));
    for(int i = 0; i < MAX_NEARBY_NODE; i++) {
        if(!strcmp(ble_nodes[i].name, item->name)) // same name - avoid duplicates
        {
            return i;
        }
    }
    return -1;
}

static int set_node_data(const char* local_name, const short rssi, ble_node_t* item) {
    if (sscanf(local_name, "[%hhu] %28s", &(item->id), item->name) == 2)
    {
        if (item->id > 0 && item->id < 8){
            item->rssi = rssi;
            item->active = true;
            item->last_found = xTaskGetTickCount();
            return 0;
        }
    }
    return -1;
}

static void sort_nodes(){
    for(int i=0; i< MAX_NEARBY_NODE-1; i++){
        if((ble_nodes[i].rssi < ble_nodes[i+1].rssi) && ble_nodes[i+1].active)
        {
            ble_node_t tmp = ble_nodes[i];
            ble_nodes[i] = ble_nodes[i+1];
            ble_nodes[i+1] = tmp;
        }
    }
}

static void disable_nodes(uint32_t now) {
    for(int i = 0; i < MAX_NEARBY_NODE; i++) {
        if(!ble_nodes[i].active) continue; // Ignore if already disabled
        if(pdTICKS_TO_MS(now - ble_nodes[i].last_found) > NODE_QUEUE_TIMEOUT_MS){
            ESP_LOGI(__FILE__, "Disabling node %s for inactivity", ble_nodes[i].name);
            ble_nodes[i].active = false;
            ble_nodes[i].name[0] = '\0';
        }
    }
}

static void insert(const char* local_name, uint8_t name_len, short rssi){
    if((name_len-4) > BADGE_BUF_SIZE) return;

    ble_node_t item;
    int res = set_node_data(local_name, rssi, &item);
    if(!res){
        ESP_LOGI(__FILE__, "[BLE node] id: %d |name: %s (%d bytes) | rssi: %d", item.id, item.name, strlen(item.name), item.rssi);
        int pos = check_duplicate(&item);
        if(pos < 0){ // No duplicates
            ESP_LOGI(__FILE__, "Node %s is new (insert & sort)", item.name);
            for(int i=0; i < MAX_NEARBY_NODE; i++) {
                if((item.rssi > ble_nodes[i].rssi) || !ble_nodes[i].active)
                {
                    ble_node_t tmp = ble_nodes[i];
                    ble_nodes[i] = item;
                    item = tmp;
                }
            }
        } else { // Duplicate
            ESP_LOGI(__FILE__, "Node %s already in pos %d (update & sort)", item.name, pos);
            ble_nodes[pos] = item;

            sort_nodes();
        }
    }
}

static void controller_rcv_pkt_ready(void)
{
    ESP_LOGI(__FILE__, "controller rcv pkt ready");
}

/*
 * @brief: BT controller callback function to transfer data packet to
 *         the host
 */
static int host_rcv_pkt(uint8_t *data, uint16_t len)
{
    host_rcv_data_t send_data;
    uint8_t *data_pkt;
    /* Check second byte for HCI event. If event opcode is 0x0e, the event is
     * HCI Command Complete event. Sice we have recieved "0x0e" event, we can
     * check for byte 4 for command opcode and byte 6 for it's return status. */
    if (data[1] == 0x0e) {
        if (data[6] == 0) {
            ESP_LOGI(__FILE__, "Event opcode 0x%02x success.", data[4]);
        } else {
            ESP_LOGE(__FILE__, "Event opcode 0x%02x fail with reason: 0x%02x.", data[4], data[6]);
            return ESP_FAIL;
        }
    }

    data_pkt = (uint8_t *)malloc(sizeof(uint8_t) * len);
    if (data_pkt == NULL) {
        ESP_LOGE(__FILE__, "Malloc data_pkt failed!");
        return ESP_FAIL;
    }
    memcpy(data_pkt, data, len);
    send_data.q_data = data_pkt;
    send_data.q_data_len = len;
    if (xQueueSend(adv_queue, (void *)&send_data, ( TickType_t ) 0) != pdTRUE) {
        ESP_LOGD(__FILE__, "Failed to enqueue advertising report. Queue full.");
        /* If data sent successfully, then free the pointer in `xQueueReceive'
         * after processing it. Or else if enqueue in not successful, free it
         * here. */
        free(data_pkt);
    }
    return ESP_OK;
}

static esp_vhci_host_callback_t vhci_host_cb = {
    controller_rcv_pkt_ready,
    host_rcv_pkt
};

static void hci_cmd_send_reset(void)
{
    uint16_t sz = make_cmd_reset (hci_cmd_buf);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
}

static void hci_cmd_send_set_evt_mask(void)
{
    /* Set bit 61 in event mask to enable LE Meta events. */
    uint8_t evt_mask[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20};
    uint16_t sz = make_cmd_set_evt_mask(hci_cmd_buf, evt_mask);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
}

static void hci_cmd_send_ble_scan_params(void)
{
    /* Set scan type to 0x01 for active scanning and 0x00 for passive scanning. */
    uint8_t scan_type = 0x01;

    /* Scan window and Scan interval are set in terms of number of slots. Each slot is of 625 microseconds. */
    uint16_t scan_interval = BLE_SCAN_INTERVAL;//0x50; /* 50 ms */
    uint16_t scan_window = BLE_SCAN_WINDOW;//0x30; /* 30 ms */

    uint8_t own_addr_type = 0x00; /* Public Device Address (default). */
    uint8_t filter_policy = 0x00; /* Accept all packets excpet directed advertising packets (default). */
    uint16_t sz = make_cmd_ble_set_scan_params(hci_cmd_buf, scan_type, scan_interval, scan_window, own_addr_type, filter_policy);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
}

static void hci_cmd_send_ble_scan_start(void)
{
    uint8_t scan_enable = 0x01; /* Scanning enabled. */
    uint8_t filter_duplicates = 0x00; /* Duplicate filtering disabled. */
    uint16_t sz = make_cmd_ble_set_scan_enable(hci_cmd_buf, scan_enable, filter_duplicates);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
    ESP_LOGI(__FILE__, "BLE Scanning started..");
}

static void hci_cmd_send_ble_adv_start(void)
{
    uint16_t sz = make_cmd_ble_set_adv_enable (hci_cmd_buf, 1);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
    ESP_LOGI(__FILE__, "BLE Advertising started..");
}

static void hci_cmd_send_ble_set_adv_param(void)
{
    /* Minimum and maximum Advertising interval are set in terms of slots. Each slot is of 625 microseconds. */
    uint16_t adv_intv_min = BLE_ADV_MIN; //0x640;   // save power, 1s boardcast once.
    uint16_t adv_intv_max = BLE_ADV_MAX;//0x640;

    /* Connectable undirected advertising (ADV_IND). */
    uint8_t adv_type = 0;

    /* Own address is public address. */
    uint8_t own_addr_type = 0;

    /* Public Device Address */
    uint8_t peer_addr_type = 0;
    uint8_t peer_addr[6] = {0x80, 0x81, 0x82, 0x83, 0x84, 0x85};

    /* Channel 37, 38 and 39 for advertising. */
    uint8_t adv_chn_map = 0x07;

    /* Process scan and connection requests from all devices (i.e., the White List is not in use). */
    uint8_t adv_filter_policy = 0;

    uint16_t sz = make_cmd_ble_set_adv_param(hci_cmd_buf,
                  adv_intv_min,
                  adv_intv_max,
                  adv_type,
                  own_addr_type,
                  peer_addr_type,
                  peer_addr,
                  adv_chn_map,
                  adv_filter_policy);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
}

static void hci_cmd_send_ble_set_adv_data(void)
{
    
    char adv_name[32];
    snprintf(adv_name, 32, "[%u] %s", badge_obj.device_id, badge_obj.device_name);

    ESP_LOGI(__FILE__, "DEVICE ID = %d", badge_obj.device_id);
    ESP_LOGI(__FILE__, "ADV NAME = %s", adv_name);
    
    uint8_t name_len = (uint8_t)strlen(adv_name);
    uint8_t adv_data[31] = {0x02, 0x01, 0x06, 0x0, 0x09};
    uint8_t adv_data_len;

    adv_data[3] = name_len + 1;
    for (int i = 0; i < name_len; i++) {
        adv_data[5 + i] = (uint8_t)adv_name[i];
    }
    adv_data_len = 5 + adv_data[3];

    uint16_t sz = make_cmd_ble_set_adv_data(hci_cmd_buf, adv_data_len, (uint8_t *)adv_data);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
    ESP_LOGI(__FILE__, "Starting BLE advertising with name \"%s\"", adv_name);
}

static esp_err_t get_local_name (uint8_t *data_msg, uint8_t data_len, ble_scan_local_name_t *scanned_packet)
{
    uint8_t curr_ptr = 0, curr_len, curr_type;
    while (curr_ptr < data_len) {
        curr_len = data_msg[curr_ptr++];
        curr_type = data_msg[curr_ptr++];
        if (curr_len == 0) {
            return ESP_FAIL;
        }

        /* Search for current data type and see if it contains name as data (0x08 or 0x09). */
        if (curr_type == 0x08 || curr_type == 0x09) {
            for (uint8_t i = 0; i < curr_len - 1; i += 1) {
                scanned_packet->scan_local_name[i] = data_msg[curr_ptr + i];
            }
            scanned_packet->name_len = curr_len - 1;
            return ESP_OK;
        } else {
            /* Search for next data. Current length includes 1 octate for AD Type (2nd octate). */
            curr_ptr += curr_len - 1;
        }
    }
    return ESP_FAIL;
}

void bt_init()
{
    bool continue_commands = 1;
    int cmd_cnt = 0;
    esp_err_t ret;
    
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret) {
        ESP_LOGI(__FILE__, "Bluetooth controller release classic bt memory failed: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGI(__FILE__, "Bluetooth controller initialize failed: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_BLE)) != ESP_OK) {
        ESP_LOGI(__FILE__, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return;
    }

    /* A queue for storing received HCI packets. */
    adv_queue = xQueueCreate(15, sizeof(host_rcv_data_t));
    if (adv_queue == NULL) {
        ESP_LOGE(__FILE__, "Queue creation failed\n");
        return;
    }

    esp_vhci_host_register_callback(&vhci_host_cb);
    while (continue_commands) {
        if (continue_commands && esp_vhci_host_check_send_available()) {
            switch (cmd_cnt) {
                case 0: hci_cmd_send_reset(); ++cmd_cnt; break;
                case 1: hci_cmd_send_set_evt_mask(); ++cmd_cnt; break;

                /* Send advertising commands. */
                case 2: hci_cmd_send_ble_set_adv_param(); ++cmd_cnt; break;
                case 3: hci_cmd_send_ble_set_adv_data(); ++cmd_cnt; break;
                case 4: hci_cmd_send_ble_adv_start(); ++cmd_cnt; break;

                /* Send scan commands. */
                case 5: hci_cmd_send_ble_scan_params(); ++cmd_cnt; break;
                case 6: hci_cmd_send_ble_scan_start(); ++cmd_cnt; break;
                default: continue_commands = 0; break;
            }
            ESP_LOGI(__FILE__, "BLE Advertise, cmd_sent: %d", cmd_cnt);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    init_ble_nodes();
}

void bt_task(void *unused)
{    
    host_rcv_data_t *rcv_data = (host_rcv_data_t *)malloc(sizeof(host_rcv_data_t));
    if (rcv_data == NULL) {
        ESP_LOGE(__FILE__, "Malloc rcv_data failed!");
        return;
    }
    esp_err_t ret;

    while (1) {
        uint8_t sub_event, num_responses, total_data_len, data_msg_ptr, hci_event_opcode;
        uint8_t *queue_data = NULL, *event_type = NULL, *addr_type = NULL, *addr = NULL, *data_len = NULL, *data_msg = NULL;
        short int *rssi = NULL;
        uint16_t data_ptr;
        ble_scan_local_name_t *scanned_name = NULL;
        total_data_len = 0;
        data_msg_ptr = 0;

        disable_nodes(xTaskGetTickCount());

        if (xQueueReceive(adv_queue, rcv_data, pdMS_TO_TICKS(NODE_QUEUE_TIMEOUT_MS)) == pdPASS) {
            /* `data_ptr' keeps track of current position in the received data. */
            data_ptr = 0;
            queue_data = rcv_data->q_data;

            /* Parsing `data' and copying in various fields. */
            hci_event_opcode = queue_data[++data_ptr];
            if (hci_event_opcode == LE_META_EVENTS) {
                /* Set `data_ptr' to 4th entry, which will point to sub event. */
                data_ptr += 2;
                sub_event = queue_data[data_ptr++];
                /* Check if sub event is LE advertising report event. */
                if (sub_event == HCI_LE_ADV_REPORT) {

                    scanned_count += 1;

                    /* Get number of advertising reports. */
                    num_responses = queue_data[data_ptr++];
                
                    if(num_responses > MAX_NEARBY_NODE)
                        num_responses = MAX_NEARBY_NODE;

                    event_type = (uint8_t *)malloc(sizeof(uint8_t) * num_responses);
                    if (event_type == NULL) {
                        ESP_LOGE(__FILE__, "Malloc event_type failed!");
                        goto reset;
                    }
                    for (uint8_t i = 0; i < num_responses; i += 1) {
                        event_type[i] = queue_data[data_ptr++];
                    }

                    /* Get advertising type for every report. */
                    addr_type = (uint8_t *)malloc(sizeof(uint8_t) * num_responses);
                    if (addr_type == NULL) {
                        ESP_LOGE(__FILE__, "Malloc addr_type failed!");
                        goto reset;
                    }
                    for (uint8_t i = 0; i < num_responses; i += 1) {
                        addr_type[i] = queue_data[data_ptr++];
                    }

                    /* Get BD address in every advetising report and store in
                     * single array of length `6 * num_responses' as each address
                     * will take 6 spaces. */
                    addr = (uint8_t *)malloc(sizeof(uint8_t) * 6 * num_responses);
                    if (addr == NULL) {
                        ESP_LOGE(__FILE__, "Malloc addr failed!");
                        goto reset;
                    }
                    for (int i = 0; i < num_responses; i += 1) {
                        for (int j = 0; j < 6; j += 1) {
                            addr[(6 * i) + j] = queue_data[data_ptr++];
                        }
                    }

                    /* Get length of data for each advertising report. */
                    data_len = (uint8_t *)malloc(sizeof(uint8_t) * num_responses);
                    if (data_len == NULL) {
                        ESP_LOGE(__FILE__, "Malloc data_len failed!");
                        goto reset;
                    }
                    for (uint8_t i = 0; i < num_responses; i += 1) {
                        data_len[i] = queue_data[data_ptr];
                        total_data_len += queue_data[data_ptr++];
                    }

                    if (total_data_len != 0) {
                        /* Get all data packets. */
                        data_msg = (uint8_t *)malloc(sizeof(uint8_t) * total_data_len);
                        if (data_msg == NULL) {
                            ESP_LOGE(__FILE__, "Malloc data_msg failed!");
                            goto reset;
                        }
                        for (uint8_t i = 0; i < num_responses; i += 1) {
                            for (uint8_t j = 0; j < data_len[i]; j += 1) {
                                data_msg[data_msg_ptr++] = queue_data[data_ptr++];
                            }
                        }
                    }

                    /* Counts of advertisements done. This count is set in advertising data every time before advertising. */
                    rssi = (short int *)malloc(sizeof(short int) * num_responses);
                    if (data_len == NULL) {
                        ESP_LOGE(__FILE__, "Malloc rssi failed!");
                        goto reset;
                    }
                    for (uint8_t i = 0; i < num_responses; i += 1) {
                        rssi[i] = -(0xFF - queue_data[data_ptr++]);
                    }

                    /* Extracting advertiser's name. */
                    data_msg_ptr = 0;
                    scanned_name = (ble_scan_local_name_t *)malloc(num_responses * sizeof(ble_scan_local_name_t));
                    if (data_len == NULL) {
                        ESP_LOGE(__FILE__, "Malloc scanned_name failed!");
                        goto reset;
                    }
                    for (uint8_t i = 0; i < num_responses; i += 1) {
                        ret = get_local_name(&data_msg[data_msg_ptr], data_len[i], scanned_name);

                        /* Print the data if adv report has a valid name. */
                        if (ret == ESP_OK) {
                            scanned_name->scan_local_name[scanned_name->name_len] = '\0';
                            //add_ble_node(scanned_name->scan_local_name, scanned_name->name_len, rssi[i]);
                            insert(scanned_name->scan_local_name, scanned_name->name_len, rssi[i]);
                        }
                    }

                    /* Freeing all spaces allocated. */
reset:
                    //ESP_LOGI(__FILE__, "BT ADV freeing memory");
                    free(scanned_name);
                    free(rssi);
                    free(data_msg);
                    free(data_len);
                    free(addr);
                    free(addr_type);
                    free(event_type);
                }
            }
            free(queue_data);
        }
        memset(rcv_data, 0, sizeof(host_rcv_data_t));
    }
}
