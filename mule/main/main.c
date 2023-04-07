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
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

// BLE headers
// TODO: non-volatile storage headers?
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "blecent.h"
#include "esp_central.h"

// mbedtls and/or crypto headers
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/platform.h"
#include "mbedtls/ssl.h"
#include "mbedtls/timing.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl_cookie.h"
#include "certs.h"
#include "time.h"

struct ble_hs_adv_fields;
struct ble_gap_conn_desc;
struct ble_hs_cfg;
union ble_store_value;
union ble_store_key;

// SENSOR_LAB11
// c0:98:e5:45:aa:bb
// 0x180A

#define SENSOR_SVC_UUID 0x180A // This is the UUID for the Galaxy service

static const ble_uuid_t *sensor_svc_uuid = BLE_UUID128_DECLARE(
    0x70, 0x6C, 0x98, 0x41, 0xCE, 0x43, 0x14, 0xA9,
    0xB5, 0x4D, 0x22, 0x2B, 0x89, 0x10, 0xE6, 0x32
);

static const ble_uuid_t *sensor_chr_uuid = BLE_UUID128_DECLARE(
    0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11,
    0x22, 0x22, 0x22, 0x22, 0x33, 0x33, 0x33, 0x33
);

static const char *tag = "MULE_LAB11"; // The Mule is an ESP32 device
static int mule_ble_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t peer_addr[6];

uint16_t ble_conn_handle;

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

/**
 * Initiates the GAP general discovery procedure.
 */
static void
sensor_scan(void)
{
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    int rc;

    //Figure out address to use while advertising TODO: change this??
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    //Tell the controller to filter duplicates
    disc_params.filter_duplicates = 1;

    //Perform a passive scan
    disc_params.passive = 1;

    //Use defaults for the rest of the parameters. 
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, 5000, &disc_params,
                      mule_ble_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error initiating GAP discovery procedure; rc=%d\n",
                    rc);
    }
}

/**
 * Called when service discovery of the specified peer has completed.
 */
static void
blecent_on_disc_complete(const struct peer *peer, int status, void *arg)
{

    if (status != 0) {
        /* Service discovery failed.  Terminate the connection. */
        MODLOG_DFLT(ERROR, "Error: Service discovery failed; status=%d "
                    "conn_handle=%d\n", status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    /* Service discovery has completed successfully.  Now we have a complete
     * list of services, characteristics, and descriptors that the peer
     * supports.
     */
    MODLOG_DFLT(INFO, "Service discovery complete; status=%d "
                "conn_handle=%d\n", status, peer->conn_handle);

    /* Now perform three GATT procedures against the peer: read,
     * write, and subscribe to notifications for the ANS service.
     */
    //TODO: add back our own read / write subscribe 
    //blecent_read_write_subscribe(peer);
}

int
ble_uuid_u128(const ble_uuid_t *uuid)
{
    //assert(uuid->type == BLE_UUID_TYPE_128);

    return uuid->type == BLE_UUID_TYPE_128 ? BLE_UUID128(uuid)->value : 0;
}


/**
 * Checks if the specified advertisement looks like a galaxy sensor.
**/
static int
sensor_should_connect(const struct ble_gap_disc_desc *disc)
{
    struct ble_hs_adv_fields fields;
    int rc;
    int i;

    /* The device has to be advertising connectability. */
    if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
            disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {

        return 0;
    }

    /* Parse the advertisement data. */
    rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    if (rc != 0) {
        return rc;
    }

    //The device has to advertise support for Galaxy services (0x180a).
    for (i = 0; i < fields.num_uuids16; i++) {
        printf("uuid16 is=%x\n", ble_uuid_u16(&fields.uuids16[i].u));
        if (ble_uuid_u16(&fields.uuids16[i].u) == 0x180a) {
            return 1;
        }
    }

    return 0;
}

void mbedtls_init() {
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
}



/**
 * Connects to the sender of the specified advertisement of it looks
 * like a galaxy sensor.  A device is treated as a sensor if it advertises 
 * connectability and support for galaxy sensor service.
 */
static void
mule_connect_if_sensor(void *disc)
{
    uint8_t own_addr_type;
    int rc;
    ble_addr_t *addr;

    //Don't do anything if it is not a sensor 
    if (!sensor_should_connect((struct ble_gap_disc_desc *)disc)) {
        //printf("Not a sensor\n");
        return;
    }

    /* Scanning must be stopped before a connection can be initiated. */
    rc = ble_gap_disc_cancel();
    if (rc != 0) {
        MODLOG_DFLT(DEBUG, "Failed to cancel scan; rc=%d\n", rc);
        return;
    }

    //Figure out address to use for connect TODO: maybe remove this after mbedtls works??
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    //Try to connect the the advertiser.
    addr = &((struct ble_gap_disc_desc *)disc)->addr;

    rc = ble_gap_connect(own_addr_type, addr, 30000, NULL,
                         mule_ble_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to connect to device; addr_type=%d "
                    "addr=%s; rc=%d\n",
                    addr->type, addr_str(addr->val), rc);
        return;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that is
 * established.  blecent uses the same callback for all connections.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  blecent.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
mule_ble_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    struct ble_hs_adv_fields fields;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                     event->disc.length_data);
        if (rc != 0) {
            return 0;
        };

        //Try to connect to the advertiser if it looks like a galaxy sensor
        mule_connect_if_sensor(&event->disc);
        return 0;

    case BLE_GAP_EVENT_CONNECT:
        //A new connection was established or a connection attempt failed
        if (event->connect.status == 0) {
            //Connection successfully established
            MODLOG_DFLT(INFO, "Connection established ");

            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            print_conn_desc(&desc);
            MODLOG_DFLT(INFO, "\n");

            //Remember peer
            rc = peer_add(event->connect.conn_handle);
            if (rc != 0) {
                MODLOG_DFLT(ERROR, "Failed to add peer; rc=%d\n", rc);
                return 0;
            }

            //Perform service discovery 
            rc = peer_disc_all(event->connect.conn_handle,
                        blecent_on_disc_complete, NULL);
            if(rc != 0) {
                MODLOG_DFLT(ERROR, "Failed to discover services; rc=%d\n", rc);
                return 0;
            }

            //Save the connection handle for future reference
            ble_conn_handle = event->connect.conn_handle;

        } else {
            //Connection attempt failed; resume scanning
            MODLOG_DFLT(ERROR, "Error: Connection failed; status=%d\n",
                        event->connect.status);
            sensor_scan();
        }

        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        //Connection terminated
        MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

        //Forget about peer
        peer_delete(event->disconnect.conn.conn_handle);

        //Resume scanning
        sensor_scan();
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        MODLOG_DFLT(INFO, "discovery complete; reason=%d\n",
                    event->disc_complete.reason);
        return 0;

    // case BLE_GAP_EVENT_ENC_CHANGE:
    //     /* Encryption has been enabled or disabled for this connection. */
    //     MODLOG_DFLT(INFO, "encryption change event; status=%d ",
    //                 event->enc_change.status);
    //     rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
    //     assert(rc == 0);
    //     print_conn_desc(&desc);
    //     return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        /* Peer sent us a notification or indication. */
        MODLOG_DFLT(INFO, "received %s; conn_handle=%d attr_handle=%d "
                    "attr_len=%d\n",
                    event->notify_rx.indication ?
                    "indication" :
                    "notification",
                    event->notify_rx.conn_handle,
                    event->notify_rx.attr_handle,
                    OS_MBUF_PKTLEN(event->notify_rx.om));

        /* Attribute data is contained in event->notify_rx.om. Use
         * `os_mbuf_copydata` to copy the data received in notification mbuf */
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    // case BLE_GAP_EVENT_REPEAT_PAIRING:
    //     /* We already have a bond with the peer, but it is attempting to
    //      * establish a new secure link.  This app sacrifices security for
    //      * convenience: just throw away the old bond and accept the new link.
    //      */

    //     /* Delete the old bond. */
    //     rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
    //     assert(rc == 0);
    //     ble_store_util_delete_peer(&desc.peer_id_addr);

    //     /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
    //      * continue with the pairing operation.
    //      */
    //     return BLE_GAP_REPEAT_PAIRING_RETRY;

    default:
        return 0;
    }
}


static void
blecent_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
blecent_on_sync(void)
{
    int rc;

    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Begin scanning for a peripheral to connect to. */
    sensor_scan();

}

void mbedtls_stuff() {
    printf("Starting the mbedtls server\n");
    int error_code;

    /*
    * Initialize the RNG and the session data
    */

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
    mbedtls_timing_delay_context timer; 
    mbedtls_net_context listen_fd, client_fd;
    mbedtls_ssl_cookie_ctx cookie_ctx;

    mbedtls_net_init(&listen_fd);
    mbedtls_net_init(&client_fd);
    mbedtls_x509_crt_init(&srvcert);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_pk_init(&pkey);
    mbedtls_ssl_cookie_init(&cookie_ctx);


    /*
    * Load the certificates and private RSA key
    */
    
    // Initialize server with mule certificate
    const unsigned char *cert_data = mule_srv_crt;
    error_code = mbedtls_x509_crt_parse(
        &srvcert,
        cert_data,
        mule_srv_crt_len
    );
    if (error_code) {
        printf("error at line %d: mbedtls_x509_crt_parse returned %d\n", __LINE__, error_code);
        abort();
    }

    const unsigned char *key_data = mule_srv_key;
    error_code = mbedtls_pk_parse_key(
        &pkey,
        key_data,
        mule_srv_key_len, NULL, 0,
        mbedtls_ctr_drbg_random, &ctr_drbg
    );
    if (error_code) {
        printf("error at line %d: mbedtls_pk_parse_key returned %d\n", __LINE__, error_code);
        abort();
    }
    
    /*
    * Setup SSL stuff
    */
    
    // Setup server
    error_code = mbedtls_ssl_config_defaults(
        &conf,
        MBEDTLS_SSL_IS_SERVER,
        MBEDTLS_SSL_TRANSPORT_DATAGRAM, 
        MBEDTLS_SSL_PRESET_DEFAULT
    );
    if (error_code) {
        printf("error at line %d: mbedtls_ssl_config_defaults returned %d\n", __LINE__, error_code);
        abort();
    }

    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    //mbedtls_ssl_conf_dbg(&conf, my_debug, stdout);
    //mbedtls_ssl_conf_read_timeout(&conf, READ_TIMEOUT_MS);

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

    //waits for BLE connection to continue 
    while (ble_gap_conn_active() == 0) {
        //wait  
        printf("waiting for BLE connection\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // Set bio to call ble connection
    //mbedtls_ssl_set_bio(&ssl, ble_conn_handle, ble_write, ble_read, NULL);
    //TODO: ble_write, ble_read

    //Handshake 
    error_code = mbedtls_ssl_handshake(&ssl);
    if (error_code) {
        printf("error at line %d: mbedtls_ssl_handshake returned %d\n", __LINE__, error_code);
        abort();
    }

    mbedtls_ssl_session_reset(&ssl);
    // TODO call mbedtls_ssl_session_rest(&ssl) when new connection

    printf("mbedtls done\n");


}

void mule_host_task(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main() {

    printf("Hello!\n");

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

    // Initialize NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    printf("nvs initialized\n");

    // Initialize the NimBLE host configuration.
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(tag, "Failed to init nimble %d ", ret);
        return;
    }

    printf("nimble initialized\n");

    // Configure NimBLE host parameters
    ble_hs_cfg.reset_cb = blecent_on_reset;
    ble_hs_cfg.sync_cb = blecent_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    printf("host configured\n");

    //Init gatt and device name 
    int rc;

    rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    if (rc != 0) {
        ESP_LOGE(tag, "error initializing gatt server");
        return;
    }

    rc = ble_svc_gap_device_name_set("Lab11 Mule");
    if (rc != 0) {
        ESP_LOGE(tag, "error setting device name");
        return;
    }

    printf("peer configured and device name set\n");

    ble_store_config_init();

    //Start the muling task 
    nimble_port_freertos_init(mule_host_task);
    
    printf("started connection\n");
    
    //get connection handle 

    mbedtls_stuff();

    
    for (int i = 20; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
