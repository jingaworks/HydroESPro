#include "wifi_conn.h"


static const char *TAG = "WIFI_CONNECT";



TaskHandle_t RECONNECT_WIFI_TaskHandle = NULL;


char ip_address[16];
bool internet_available = false;

// char current_ssid[33];

bool sntp_started;


char DNS_URL[] = "google.com";
ip_addr_t DNS_ip_Addr;

esp_ip4_addr_t ip;
esp_ip4_addr_t gw;
esp_ip4_addr_t msk;

static EventGroupHandle_t s_wifi_event_group;


#define ESP_MAX_STA_CONN    2
#define ESP_MAXIMUM_RETRY   5

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

#define DNS_CONNECTED_BIT BIT3
#define DNS_FAIL_BIT BIT4

static int s_retry_num = 0;

char user_wifi_cc[45][3] = {"01","AT","AU","BE","BG","BR","CA","CH","CN","CY","CZ","DE","DK","EE","ES","FI","FR","GB","GR","HK","HR","HU","IE","IN","IS","IT","JP","KR","LI","LT","LU","LV","MT","MX","NL","NO","NZ","PL","PT","RO","SE","SI","SK","TW","US"};
uint8_t user_wifi_cc_index;

char ip_address[16];
bool wifi_connected;


esp_netif_t *netif_ap;
esp_netif_t *netif_sta;




void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    if ( ipaddr != NULL ) {
        DNS_ip_Addr = *ipaddr;
        xEventGroupSetBits(s_wifi_event_group, DNS_CONNECTED_BIT);
    }
    else xEventGroupSetBits(s_wifi_event_group, DNS_FAIL_BIT);
}

/***********************************************/
/*** WiFi Status *******************************/
static void sta_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_BEACON_TIMEOUT)
    {
        ESP_LOGI(TAG, "WIFI_EVENT_STA_BEACON_TIMEOUT");
    }
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Try reconnecting to AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }

        ESP_LOGI(TAG, "DISCONNECTED from AP");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
static void softap_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station %02x:%02x:%02x:%02x:%02x:%02x join, AID=%d", 
                                event->mac[0], 
                                event->mac[1], 
                                event->mac[2], 
                                event->mac[3], 
                                event->mac[4], 
                                event->mac[5], 
                                event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station %02x:%02x:%02x:%02x:%02x:%02x leave, AID=%d", 
                                event->mac[0], 
                                event->mac[1], 
                                event->mac[2], 
                                event->mac[3], 
                                event->mac[4], 
                                event->mac[5], 
                                event->aid);
    }
    else
        ESP_LOGI(TAG, "station AID=%ld", event_id);
}

static esp_err_t wifi_deinit(esp_netif_t * netif)
{
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_STA)
    {
        ESP_LOGI(TAG, "mode == WIFI_MODE_STA");
        
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &sta_event_handler));
        ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &sta_event_handler));
        ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_BEACON_TIMEOUT, &sta_event_handler));

        esp_netif_destroy_default_wifi(netif);
        
        ESP_ERROR_CHECK(esp_wifi_deinit());
        ESP_ERROR_CHECK(esp_event_loop_delete_default());

    }
    else if (mode == WIFI_MODE_AP)
    {
        ESP_LOGI(TAG, "mode == WIFI_MODE_AP");
        
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &softap_event_handler));
        
        esp_netif_destroy_default_wifi(netif);
        
        ESP_ERROR_CHECK(esp_wifi_deinit());
        ESP_ERROR_CHECK(esp_event_loop_delete_default());
    }
    return ESP_OK;
}

void start_wifi_ap(void)
{
    // LOG_DATA WIFI_AP_INIT_START

    ESP_LOGI(TAG, "wifi init AP.");

    ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    netif_ap = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &softap_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_AP_SSID,
            .password = ESP_WIFI_AP_PASS,
            .max_connection = ESP_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(ESP_IF_WIFI_AP, WIFI_BW_HT20));
    ESP_ERROR_CHECK(esp_wifi_start());

    // LOG_DATA WIFI_AP_START_OK
    
    ESP_LOGI(TAG, "wifi init softap finished. SSID:%s password:%s", ESP_WIFI_AP_SSID, ESP_WIFI_AP_PASS);
    start_http_server();
}
esp_err_t start_wifi_sta(void)
{
    // LOG_DATA WIFI_STA_INIT_START
    
    ESP_LOGI(TAG, "wifi init STA.");
    
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    netif_sta = esp_netif_create_default_wifi_sta();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        WIFI_EVENT_STA_BEACON_TIMEOUT,
                                                        &sta_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &sta_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &sta_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t sta_wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strcpy((char *)sta_wifi_config.sta.ssid, "TPLink_G");
    strcpy((char *)sta_wifi_config.sta.password, "L30nt3_123");
    

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(ESP_IF_WIFI_STA, WIFI_BW_HT20));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        // LOG_DATA WIFI_STA_CONNECT_SUCCESS

        wifi_connected = true;
        ESP_LOGI(TAG, "Connected to AP");
        
        wifi_connected = true;
        if (RECONNECT_WIFI_TaskHandle == NULL)
            start_reconnect_wifi_task();

        start_http_server();
        
        // LOG_DATA DNS_LOOKUP_START

        IP_ADDR4( &DNS_ip_Addr, 0,0,0,0 );

        // ESP_LOGI( TAG, "Get IP for URL: %s", DNS_URL );

        err_t dns_ret = dns_gethostbyname( DNS_URL, &DNS_ip_Addr, dns_found_cb, NULL );
        if ( dns_ret != ERR_INPROGRESS ) {
            
            // LOG_DATA DNS_LOOKUP_ERROR
            
            ESP_LOGE( TAG, "DNS Lookup Error: %d", dns_ret );
            
            xEventGroupSetBits( s_wifi_event_group, DNS_FAIL_BIT );
        }

        /* Waiting until either the lookup succeed ( dns_found_cb() ) -> ( DNS_CONNECTED_BIT ) 
        or lookup failed ( dns_ret != ERR_INPROGRESS ) -> ( DNS_FAIL_BIT ). 
        The bits are set by ( dns_found_cb() ) or ( dns_ret != ERR_INPROGRESS ) (see above) */
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                        DNS_CONNECTED_BIT | DNS_FAIL_BIT,
                                        pdTRUE,
                                        pdFALSE,
                                        portMAX_DELAY);

        /* xEventGroupWaitBits() returns the bits before the call returned, 
        hence we can test which event actually happened. */
        if ( bits & DNS_CONNECTED_BIT ) {
            
            // LOG_DATA DNS_LOOKUP_SUCCESS
            
            // ESP_LOGI(TAG , "%s ip is " IPSTR, DNS_URL, IP2STR(&DNS_ip_Addr));

            internet_available = true;
            dns_clear_cache();
            if ( !sntp_started ) {

                // LOG_DATA SNTP_START
                
                sntp_started = true;
                
                // obtain_time();
            }
            ESP_LOGI( TAG, "DNS Connected" );
        }
        else if ( bits & DNS_FAIL_BIT ) {

            // LOG_DATA DNS_LOOKUP_FAIL
            
            internet_available = false;
            
            ESP_LOGE( TAG, "DNS Not Connected" );
        }
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        // LOG_DATA WIFI_STA_CONNECT_FAIL

        wifi_connected = false;
        
        ESP_LOGI(TAG, "Failed to connect to AP");

        wifi_deinit(netif_sta);
        start_wifi_ap();

        return ESP_FAIL;
    }
    else
    {
        // LOG_DATA WIFI_STA_CONNECT_ERROR

        ESP_LOGE(TAG, "UNEXPECTED EVENT");

        return ESP_FAIL;
    }

    // LOG_DATA WIFI_STA_CONNECT_OK
    return ESP_OK;
}

static void reconnect_wifi_sta_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Start: reconnect_wifi_sta_task");

    for (;;)
    {
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                               WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                               pdTRUE,
                                               pdFALSE,
                                               portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            wifi_connected = true;
        }
        else if (bits & WIFI_FAIL_BIT)
        {
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            wifi_connected = false;

            esp_wifi_connect();
        }
        else
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}
esp_err_t start_reconnect_wifi_task(void)
{
    xTaskCreate(reconnect_wifi_sta_task, "reconect_wifi", 1024 * 4, NULL, 2, &RECONNECT_WIFI_TaskHandle);
    if (RECONNECT_WIFI_TaskHandle == NULL)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}
