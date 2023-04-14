
#include "util.h"

#include <stdio.h>
#include <inttypes.h>

#include "esp_chip_info.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_flash.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "wifi_credentials_DO_NOT_COMMIT.h"

#define WIFI_CONN_MAX_RETRY 10
#define NETIF_DESC_STA "Mule Wifi"

static esp_netif_t *wifi_sta_netif = NULL;
static SemaphoreHandle_t wifi_get_ip_addrs_semph = NULL;
static int retry_num = 0;

/*
 * Prints chip information.
 */
void print_chip_info() {

    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());    
}

/*
 * Initialize NVS for Wifi/BT. Returns status code.
 */
void init_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

/*
 * WiFi Shenanigans >:)
 */

static void wifi_disconnect_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data) {
    retry_num++;
    if (retry_num > WIFI_CONN_MAX_RETRY) {
        printf("Failed to connect to AP after %d retries\n", retry_num);
        if (wifi_get_ip_addrs_semph) {
            xSemaphoreGive(wifi_get_ip_addrs_semph);
        }
        return;
    }

    printf("Wifi disconnected, retrying...\n");
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
        return;
    }
    ESP_ERROR_CHECK(err);
}

static void wifi_got_ip_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {

    retry_num = 0;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;

    printf("Got IPv4 event: Interface \"%s\" address: " IPSTR "\n",
           esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    
    if (wifi_get_ip_addrs_semph) {
        xSemaphoreGive(wifi_get_ip_addrs_semph);
    } else {
        printf("IPv4 address: " IPSTR "\n", IP2STR(&event->ip_info.ip));
    }
}

static void wifi_connected_handler(void* arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data) {
    printf("Wifi connected\n");
}

static void start_wifi() {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_inherent_config_t netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    netif_config.if_desc = NETIF_DESC_STA;
    netif_config.route_prio = 128;
    wifi_sta_netif = esp_netif_create_wifi(WIFI_IF_STA, &netif_config);
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

esp_err_t init_wifi() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    start_wifi();
    printf("Wifi started\n");

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };

    wifi_get_ip_addrs_semph = xSemaphoreCreateBinary();
    if (wifi_get_ip_addrs_semph == NULL) {
        return ESP_ERR_NO_MEM;
    }

    retry_num = 0;

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_disconnect_handler, NULL
    ));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_got_ip_handler, NULL
    ));
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &wifi_connected_handler, NULL
    ));

    printf("Connecting to SSID %s...\n", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    esp_err_t error_code = esp_wifi_connect();
    if (error_code != ESP_OK) {
        printf("Error connecting to SSID %s: %s\n", wifi_config.sta.ssid, esp_err_to_name(error_code));
        return error_code;
    }

    printf("Waiting for IP address...\n");
    xSemaphoreTake(wifi_get_ip_addrs_semph, portMAX_DELAY);
    if (retry_num > WIFI_CONN_MAX_RETRY) {
        printf("Waiting for an IP timed out\n");
        return ESP_FAIL;
    }

    return ESP_OK;
}