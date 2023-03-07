/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

// BLE headers
// TODO: non-volatile storage headers?
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "blecent.h"
//#include "esp_central.h"

// mbedtls and/or crypto headers
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/platform.h"
#include "mbedtls/ssl.h"

struct ble_hs_adv_fields;
struct ble_gap_conn_desc;
struct ble_hs_cfg;
union ble_store_value;
union ble_store_key;

// TESS_LAB11
// c0:98:e5:45:aa:bb

//#define BLECENT_SVC_UUID 0x1811 // XXX

static const ble_uuid_t *sensor_svc_uuid = BLE_UUID128_DECLARE(
    0x70, 0x6C, 0x98, 0x41, 0xCE, 0x43, 0x14, 0xA9,
    0xB5, 0x4D, 0x22, 0x2B, 0x89, 0x10, 0xE6, 0x32
);

static const ble_uuid_t *sensor_chr_uuid = BLE_UUID128_DECLARE(
    0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11,
    0x22, 0x22, 0x22, 0x22, 0x33, 0x33, 0x33, 0x33
);

static const char *tag = "JL_LAB11";
static int mule_ble_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t peer_addr[6];

void ble_store_config_init();

/*
static int mule_ble_on_read(uint16_t conn_handle, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg) {

    printf("Read done for subscribable characteristic | status=%d conn_handle=%d\n",
            error->status, conn_handle);

    if (error->status == 0) {
        printf("  attr_handle=%d value=", attr->handle);
        print_mbuf(attr->om); 
    }
    printf("\n");

    return 0;
}

static int mule_ble_on_write(uint16_t conn_handle, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg) {

    printf("Write made for subscribable characteristic | status=%d conn_handle=%d attr_handle=%d\n",
            error->status, conn_handle, attr->handle);

    const struct peer *peer = peer_find(conn_handle);
    const struct peer_chr *chr = peer_chr_find_uuid(peer, sensor_svc_uuid, sensor_chr_uuid);
    if (chr == NULL) {
        printf("Error: peer doesn't have the subscribable characteristic\n");
        return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }

    int rc = ble_gattc_read(conn_handle, chr->chr.val_handle, mule_ble_on_read, NULL);
    if (rc != 0) {
        printf("Error: failed to read the subscribable characteristic\n");
        return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }

    return 0;
}

static int mule_ble_on_subscribe(uint16_t conn_haandle, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg) {

    printf("Subscribe complete to subscribable characteristic | status=%d conn_handle=%d\n",
            error->status, conn_handle);
    
    if (error->status == 0) {
        printf("  attr_handle=%d value=", attr->handle);
        print_mbuf(attr->om);
    }
    printf("\n");

    const struct peer *peer = peer_find(conn_handle);
    const struct peer_chr *chr = peer_chr_find_uuid(peer, sensor_svc_uuid, sensor_chr_uuid);
    if (chr == NULL) {
        printf("Error: peer doesn't have the subscribable characteristic\n");
        return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }

    // write a test value to characteristic to see if it notifies us
    uint8_t value = 0x19;
    int rc = ble_gattc_write_flat(conn_handle, chr->chr.val_handle, &value, sizeof(value), mule_ble_on_write, NULL);
    if (rc != 0) {
        printf("Error: failed to write to the subscribable characteristic | rc=%d\n", rc);
        return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }

    return 0;
}
*/

void mbedtls_stuff() {
    printf("-- Trying out MBED TLS server (w/out BLE connection) --\n");
    int error_code;

    // initialize entropy and seed random generator
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg; 

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    if ((error_code = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0)) != 0) {
        printf("error at line %d: mbedtls_ctr_drbg_seed returned %d\n", __LINE__, error_code);
        abort();
    }

    // initialize TLS server parameters
    mbedtls_x509_crt srvcert;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_pk_context pkey;

    mbedtls_x509_crt_init(&srvcert);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_pk_init(&pkey);

    // TODO something about setting up network connections
    
    // Initialize server with mule certificate
    // XXX: currently uses embedded test certs
    /*
    error_code = mbedtls_x509_crt_parse(
        &srvcert,
        (const unsigned char *) mbedtls_test_srv_crt,
        mbedtls_test_srv_crt_len
    );
    if (error_code) {
        printf("error at line %d: mbedtls_x509_crt_parse returned %d\n", __LINE__, error_code);
        abort();
    }

    error_code = mbedtls_x509_crt_parse(
        &srvcert,
        (const unsigned char *) mbedtls_test_cas_pem,
        mbedtls_test_cas_pem_len
    );
    if (error_code) {
        printf("error at line %d: mbedtls_x509_crt_parse returned %d\n", __LINE__, error_code);
        abort();
    }

    error_code = mbedtls_pk_parse_key(
        &pkey,
        (const unsigned char *) mbedtls_test_srv_key,
        mbedtls_test_srv_key_len, NULL, 0,
        mbedtls_ctr_drbg_random, &ctr_drbg
    );
    if (error_code) {
        printf("error at line %d: mbedtls_pk_parse_key returned %d\n", __LINE__, error_code);
        abort();
    }
    */

    // TODO start listening on "socket"
    
    // Setup server
    error_code = mbedtls_ssl_config_defaults(
        &conf,
        MBEDTLS_SSL_IS_SERVER,
        MBEDTLS_SSL_TRANSPORT_STREAM, // _DATAGRAM for DTLS, perhaps?
        MBEDTLS_SSL_PRESET_DEFAULT
    );
    if (error_code) {
        printf("error at line %d: mbedtls_ssl_config_defaults returned %d\n", __LINE__, error_code);
        abort();
    }

    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    // mbedtls_ssl_conf_dbg ??

    mbedtls_ssl_conf_ca_chain(&conf, srvcert.next, NULL);
    error_code = mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey);
    if (error_code) {
        printf("error at line %d: mbedtls_ssl_conf_own_cert returned %d\n", __LINE__, error_code);
        abort();
    }
    
    error_code = mbedtls_ssl_setup(&ssl, &conf);
    if (error_code) {
        printf("error at line %d: mbedtls_ssl_setup returned %d\n", __LINE__, error_code);
        abort();
    }

    mbedtls_ssl_session_reset(&ssl);
    // TODO call mbedtls_ssl_session_rest(&ssl) when new connection

    printf("--             done               --\n");


}

void app_main() {

    printf("Hello world!\n");

    /* Print chip information */
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

    mbedtls_stuff();
    
    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
