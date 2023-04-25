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
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"

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
//#include "certs.h"
#include "time.h"

#include "backhaul.h"
#include "util.h"

struct ble_hs_adv_fields;
struct ble_gap_conn_desc;
struct ble_hs_cfg;
union ble_store_value;
union ble_store_key;

// SENSOR_LAB11
// c0:98:e5:45:aa:bb
// 0x180A

#define SENSOR_SVC_UUID 0x180A // This is the UUID for the Galaxy service

static const ble_uuid_t *sensor_chr_uuid = BLE_UUID128_DECLARE(
    0x32, 0xE6, 0x10, 0x89, 0x2B, 0x22, 0x4D, 0xB5,
    0xA9, 0x14, 0x43, 0xCE, 0x41, 0x98, 0x6C, 0x70
);

#define LE_PHY_UUID16               0xABF3
#define LE_PHY_CHR_UUID16           0xF2AB

//static const ble_uuid_t *sensor_chr_uuid = BLE_UUID16_DECLARE(0x8911);

static const char *tag = "MULE_LAB11"; // The Mule is an ESP32 device
static int mule_ble_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t peer_addr[6];

uint16_t ble_conn_handle;

void ble_store_config_init();

// A little buffer and pointer array to hold our payloads.
//static char payload_buff[200];
//static char *payload_starts[10];

// A little buffer and pointer array to hold our tokens
static int num_stored_tokens = 0;
static char token_buff[880] = {0}; // each token is 88 bytes
static char *token_starts[11]; // we can store up to 10 tokens

/*
* App call back for read of characteristic has completed
*/
static int ble_on_read(uint16_t conn_handle, const struct ble_gatt_error *error,
                        struct ble_gatt_attr *attr, void *arg) {
    
    MODLOG_DFLT(INFO, "Read complete; status=%d conn_handle=%d", error->status,conn_handle);
    if (error->status == 0) {
        MODLOG_DFLT(INFO, " attr_handle=%d value=", attr->handle);
        print_mbuf(attr->om);
    }
    MODLOG_DFLT(INFO, "\n");

    return 0;
}

/*
* App call back for write of characteristic has completed
*/
static int ble_on_write(uint16_t conn_handle, const struct ble_gatt_error *error,
                        struct ble_gatt_attr *attr, void *arg) {

    MODLOG_DFLT(INFO,"Write complete; status=%d conn_handle=%d attr_handle=%d\n",
                error->status, conn_handle, attr->handle);
    return 0;
}

/*
* App call back for subscribe to characteristic has completed
*/
static int ble_on_subscribe(uint16_t conn_handle, const struct ble_gatt_error *error,
                            struct ble_gatt_attr *attr, void *arg) {

    MODLOG_DFLT(INFO, "Subscribe complete; status=%d conn_handle=%d attr_handle=%d\n",
                error->status, conn_handle, attr->handle);
    return 0;
}

static void ble_read(const struct peer *peer) {
    
    // const struct peer_chr_list *chr_list;
    // const struct peer_chr *chr;
    // int rc;

    // /* Find the UUID. */
    // printf("service UUID: %x\n", LE_PHY_UUID16);
    // printf("characteristic UUID: %x\n", LE_PHY_CHR_UUID16);
    
    // chr =  peer_chr_find_uuid(peer,
    //                          BLE_UUID16_DECLARE(LE_PHY_UUID16),
    //                          BLE_UUID16_DECLARE(LE_PHY_CHR_UUID16));
    
    // // peer_chr_find_uuid(peer, BLE_UUID16_DECLARE(SENSOR_SVC_UUID),
    // //                          BLE_UUID128_DECLARE(0x32, 0xE6, 0x10, 0x89, 
    // //                          0x2B, 0x22, 0x4D, 0xB5,
    // //                          0xA9, 0x14, 0x43, 0xCE, 
    // //                          0x41, 0x98, 0x6C, 0x70));
    // if (chr == NULL) {
    //     printf("Error: Peer doesn't support NEBULA\n");
    //     // printf("service UUID: %x\n", SENSOR_SVC_UUID);
    //     // printf("characteristic UUID: %d\n", (int)sensor_chr_uuid);
    // }

    // /* Read the characteristic. */
    // rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle,
    //                     ble_on_read, NULL);
    // if (rc != 0) {
    //     printf("Error: Failed to read characteristic; rc=%d\n", rc);
    // }
}

static void ble_write(const struct peer *peer) {

    // const struct peer_chr *chr;
    // uint8_t value[2]; //TODO: needs to be input pointer for bios
    // int rc;
    
    // /* Find the UUID. */
    // chr = peer_chr_find_uuid(peer, SENSOR_SVC_UUID, sensor_chr_uuid);
    // if (chr == NULL) {
    //     printf("Error: Peer doesn't support NEBULA\n");
    // }

    // /* Write the characteristic. */
    // value[0] = 99;
    // value[1] = 100;
    // rc = ble_gattc_write_flat(peer->conn_handle, chr->chr.val_handle,
    //                           value, sizeof(value), ble_on_write, NULL);
    // if (rc != 0) {
    //     printf("Error: Failed to write characteristic; rc=%d\n", rc);
    // }
}


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

void char_discovery_callback(uint16_t conn_handle, int status, struct ble_gatt_chr *chr, void *arg)
{
    if (status == 0) {
        // Characteristic discovery succeeded
        printf("Characteristic discovery succeeded. Characteristic UUID:");
    } else {
        // Characteristic discovery failed
        printf("Characteristic discovery failed. Status: %d\n", status);
    }
}

// Callback function to handle the result of the service discovery
void service_discovery_callback(uint16_t conn_handle, int status, struct ble_gatt_svc *service, void *arg)
{
    if (status == 0) {
        printf("I'm so tired, things are not going productively\n");
    }
        // Service discovery succeeded
        //printf("Service discovery succeeded. Service UUID: %x\n", service->uuid);
        
        // Loop through the characteristics of the discovered service
        //int rc = ble_gattc_disc_all_chrs(conn_handle, service->start_handle, service->end_handle, char_discovery_callback, NULL);
        //struct ble_gatt_chr *chr = service->chrs;
    //     struct ble_gatt_chr *chr;
    //     uint16_t handle;
    //     int rc;
    //     for (handle=service->start_handle; handle <= service->end_handle; handle++) {
    //         chr = ble_gattc_chr_find_by_val_handle(conn_handle, handle);
    //         if (chr != NULL) {
    //             continue;
    //         }
    //         else {
    //             //read the characteristic
    //             rc = ble_gattc_read(conn_handle, chr->chr.val_handle,
    //                     ble_on_read, NULL);
    //             if (rc == 0) {
    //                 printf(chr->chr.value);
    //             }
    //         }
    //     }

    //     if (rc != 0) {
    //         printf("Error: Failed to discover characteristics; rc=%d\n", rc);
    //     }
    // } else {
    //     // Service discovery failed
    //     printf("Service discovery failed. Status: %d\n", status);
    // }
}

/**
 * Called when service discovery of the specified peer has completed.
 */
static void
blecent_on_disc_complete(const struct peer *peer, int status, void *arg)
{

    // if (status != 0) {
    //     /* Service discovery failed.  Terminate the connection. */
    //     MODLOG_DFLT(ERROR, "Error: Service discovery failed; status=%d "
    //                 "conn_handle=%d\n", status, peer->conn_handle);
    //     ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    //     return;
    // }
    // /* Service discovery has completed successfully.  Now we have a complete
    //  * list of services, characteristics, and descriptors that the peer
    //  * supports.
    //  */
    
    // MODLOG_DFLT(INFO, "Service discovery complete; status=%d "
    //             "conn_handle=%d\n", status, peer->conn_handle);

    // /* Now perform three GATT procedures against the peer: read,
    //  * write, and subscribe to notifications for the ANS service.
    //  */
    // //TODO: add back our own read / write subscribe 
    // //blecent_read_write_subscribe(peer);

    // int rc = ble_gattc_disc_all_svcs(peer->conn_handle, service_discovery_callback, NULL);
    // if (rc != 0) {
    //     printf("Failed to initiate service discovery. Error: %d\n", rc);
    // }

    // ble_read(peer);
    // printf("read done\n");
}

int
ble_uuid_u128(const ble_uuid_t *uuid)
{
    //assert(uuid->type == BLE_UUID_TYPE_128);

    //return uuid->type == BLE_UUID_TYPE_128 ? BLE_UUID128(uuid)->value : 0;
    return 0;
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


static void ble_on_reset(int reason) {
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void ble_on_sync(void) {
    int rc;

    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Begin scanning for a peripheral to connect to. */
    sensor_scan();

}

void mbedtls_stuff() {
    // printf("Starting the mbedtls server\n");
    // int error_code;

    // /*
    // * Initialize the RNG and the session data
    // */

    // // initialize entropy and seed random generator
    // mbedtls_entropy_context entropy;
    // mbedtls_ctr_drbg_context ctr_drbg; 

    // mbedtls_entropy_init(&entropy);
    // mbedtls_ctr_drbg_init(&ctr_drbg);
    // if ((error_code = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0)) != 0) {
    //     printf("error at line %d: mbedtls_ctr_drbg_seed returned %d\n", __LINE__, error_code);
    //     abort();
    // }

    // // initialize TLS server parameters
    // mbedtls_x509_crt srvcert;
    // mbedtls_ssl_context ssl;
    // mbedtls_ssl_config conf;
    // mbedtls_pk_context pkey;
    // mbedtls_timing_delay_context timer; 
    // mbedtls_net_context listen_fd, client_fd;
    // mbedtls_ssl_cookie_ctx cookie_ctx;

    // mbedtls_net_init(&listen_fd);
    // mbedtls_net_init(&client_fd);
    // mbedtls_x509_crt_init(&srvcert);
    // mbedtls_ssl_init(&ssl);
    // mbedtls_ssl_config_init(&conf);
    // mbedtls_pk_init(&pkey);
    // mbedtls_ssl_cookie_init(&cookie_ctx);


    // /*
    // * Load the certificates and private RSA key
    // */
    
    // // Initialize server with mule certificate
    // // XXX: hack
    // const char *mule_srv_crt = "none";
    // const unsigned char *cert_data = mule_srv_crt;
    // error_code = mbedtls_x509_crt_parse(
    //     &srvcert,
    //     cert_data,
    //     mule_srv_crt_len
    // );
    // if (error_code) {
    //     printf("error at line %d: mbedtls_x509_crt_parse returned %d\n", __LINE__, error_code);
    //     abort();
    // }

    // const unsigned char *key_data = mule_srv_key;
    // error_code = mbedtls_pk_parse_key(
    //     &pkey,
    //     key_data,
    //     mule_srv_key_len, NULL, 0,
    //     mbedtls_ctr_drbg_random, &ctr_drbg
    // );
    // if (error_code) {
    //     printf("error at line %d: mbedtls_pk_parse_key returned %d\n", __LINE__, error_code);
    //     abort();
    // }
    
    // /*
    // * Setup SSL stuff
    // */
    
    // // Setup server
    // error_code = mbedtls_ssl_config_defaults(
    //     &conf,
    //     MBEDTLS_SSL_IS_SERVER,
    //     MBEDTLS_SSL_TRANSPORT_DATAGRAM, 
    //     MBEDTLS_SSL_PRESET_DEFAULT
    // );
    // if (error_code) {
    //     printf("error at line %d: mbedtls_ssl_config_defaults returned %d\n", __LINE__, error_code);
    //     abort();
    // }

    // mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    // //mbedtls_ssl_conf_dbg(&conf, my_debug, stdout);
    // //mbedtls_ssl_conf_read_timeout(&conf, READ_TIMEOUT_MS);

    // mbedtls_ssl_conf_ca_chain(&conf, srvcert.next, NULL);
    // error_code = mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey);
    // if (error_code) {
    //     printf("error at line %d: mbedtls_ssl_conf_own_cert returned %d\n", __LINE__, error_code);
    //     abort();
    // }
    
    // error_code = mbedtls_ssl_setup(&ssl, &conf);
    // if (error_code) {
    //     printf("error at line %d: mbedtls_ssl_setup returned %d\n", __LINE__, error_code);
    //     abort();
    // }

    // //waits for BLE connection to continue 
    // while (ble_gap_conn_active() == 0) {
    //     //wait  
    //     printf("waiting for BLE connection\n");
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }

    // Set bio to call ble connection
    //mbedtls_ssl_set_bio(&ssl, ble_conn_handle, ble_write, ble_read, NULL);


/*
    //Handshake 
    error_code = mbedtls_ssl_handshake(&ssl);
    if (error_code) {
        printf("error at line %d: mbedtls_ssl_handshake returned %d\n", __LINE__, error_code);
        abort();
    }

    mbedtls_ssl_session_reset(&ssl);
    // TODO call mbedtls_ssl_session_reset(&ssl) when new connection

    printf("mbedtls done\n");
*/


}

void mule_host_task(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();
    nimble_port_freertos_deinit();
}

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {

    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            printf("HTTP_EVENT_ERROR\n");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            printf("HTTP_EVENT_ON_CONNECTED\n");
            break;
        case HTTP_EVENT_HEADER_SENT:
            printf("HTTP_EVENT_HEADER_SENT\n");
            break;
        case HTTP_EVENT_ON_HEADER:
            printf("HTTP_EVENT_ON_HEADER, key=%s, value=%s\n", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            printf("HTTP_EVENT_ON_DATA, len=%d\n", evt->data_len);
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                int copy_len = 0;
                if (evt->user_data) {
                    copy_len = MAX_HTTP_OUTPUT_BUFFER - output_len;
                    if (copy_len > evt->data_len) {
                        copy_len = evt->data_len;
                    }

                    if (copy_len) {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    }
                } else {
                    const int buffer_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        output_buffer = (char *) malloc(buffer_len);
                        output_len = 0;
                        if (output_buffer == NULL) {
                            printf("Failed to allocate memory for output buffer\n");
                            return ESP_FAIL;
                        }
                    }

                    copy_len = buffer_len - output_len;
                    if (copy_len > evt->data_len) {
                        copy_len = evt->data_len;
                    }
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                }
                output_len += copy_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            printf("HTTP_EVENT_ON_FINISH\n");
            if (output_buffer != NULL) {
                // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
                // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            printf("HTTP_EVENT_DISCONNECTED\n");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                printf("Last esp error code: 0x%x\n", err);
                printf("Last mbedtls failure: 0x%x\n", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            printf("HTTP_EVENT_REDIRECT\n");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
    }
    return ESP_OK;
}

void http_get_test(void) {

    // Instantiate a buffer to hold our resulting data.
    char responseBuffer[200] = {0}; // I think our responses are 197 bytes long, so this should fit.
    
    esp_http_client_config_t config = {
        .host = "34.31.110.212", // ping our provider server
        .path = "/public_params",
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = _http_event_handler,
        .user_data = responseBuffer,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        printf("HTTP GET Status = %d, content_length = %"PRIu64"\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        printf("HTTP GET data = %s\n", responseBuffer);
    } else {
        printf("HTTP GET request failed: %s\n", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

// Takes in a NULL-terminated string as a payload.
void http_attempt_single_upload(char payload[], int payload_len) {
    char json_prefix[] = "{\"data\":\"";
    char json_suffix[] = "\"}";
    char post_data[200] = {0};
    int prefix_len = sizeof(json_prefix) - 1;
    int suffix_len = sizeof(json_suffix) - 1;
    int post_len = prefix_len + payload_len + suffix_len;
    
    printf("Payload = %s\n", payload);
    printf("Prefix length = %d, suffix length = %d, payload_len = %d, post_len = %d\n", prefix_len, suffix_len, payload_len, post_len);
    
    // Check to make sure the payload exists.
    if (payload_len <= 0) {
        printf("The payload is empty >:(");
        return;
    }
    
    // Check to make sure the payload isn't too long.
    if (post_len >= sizeof(post_data)) {
        printf("The payload is too long >:(");
        return;
    }
    
    // Instantiate a buffer to hold our resulting data.
    char responseBuffer[105] = {0}; // I think our responses are 101 bytes long, so this should fit.
    
    // If everything looks ok, we do a POST request.
    esp_http_client_config_t config = {
        .host = "34.27.170.95", // hehe we only have one APP server :3
        .path = "/deliver",
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = _http_event_handler,
        .user_data = responseBuffer,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // POST
    memcpy(post_data, json_prefix, prefix_len);
    memcpy(post_data + prefix_len, payload, payload_len);
    memcpy(post_data + prefix_len + payload_len, json_suffix, suffix_len);
    
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, post_len);
    
    printf("POST request = %s\n", post_data);
    /*
    for (int aChar = 0; aChar < post_len; aChar++) {
        if (post_data[aChar] == 0) {
            printf("NULL");
        } else {
            printf("%c", post_data[aChar]);
        }
    } printf("\n");
    */
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        printf("HTTP POST Status = %d, content_length = %"PRIu64"\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        printf("HTTP POST data = %s\n", responseBuffer);
        
        // Store the returned token into token_buff[880] and update token_starts[11] and num_stored_tokens accordingly.
        // Assumes that the format of the response is {"result":"token"}.
        int token_len = strlen(responseBuffer) - 13;
        
        // If we're out of pointer space, we can't store the token.
        if (num_stored_tokens >= sizeof(token_starts)) {
            printf("You already have %d tokens and cannot store any more tokens without redeeming!\n", num_stored_tokens);
        } else if (sizeof(token_buff) - (token_starts[num_stored_tokens] - token_buff) < token_len) {
        // If we're out of buffer space, we can't store the token.
            printf("We are out of token buffer space and cannot store any more tokens without redeeming!\n");
        } else {
        // If all is well, then we store the token and update our bookkeeping.
            memcpy(token_starts[num_stored_tokens], responseBuffer + 11, token_len);
            num_stored_tokens++;
            token_starts[num_stored_tokens] = token_starts[num_stored_tokens - 1] + token_len;
        }
        
    } else {
        printf("HTTP POST request failed: %s\n", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

// Takes in an array of pointers to NULL-terminated strings.
void http_attempt_many_uploads(char *payloads[], int num_payloads) {
    for (int i = 0; i < num_payloads; i++) {
        // For each pointer, we do a single POST request.
        http_attempt_single_upload(payloads[i], strlen(payloads[i]));
        // NOTE: We will have to change this when we start using Tess's code.
    }
}

// Takes in an array of pointers to NULL-terminated strings.
void http_attempt_redeem() { //char *tokens[], int num_tokens) { // Updated http_attempt_redeem() to read from global variables.
    char json_prefix[] = "{\"tokens\":[\"";
    char json_suffix[] = "\"]}";
    char json_delim[] = "\",\"";
    char post_data[1000] = {0};
    int prefix_len = sizeof(json_prefix) - 1;
    int suffix_len = sizeof(json_suffix) - 1;
    int delim_len = sizeof(json_delim) - 1;
    int token_len;
    int cur_post_length = 0;
    
    // Instantiate a buffer to hold our resulting data.
    char responseBuffer[20] = {0}; // I think our responses are very small, so this should fit.
    
    // Initiate a POST request.
    esp_http_client_config_t config = {
        .host = "34.31.110.212", // hehe we hardcode the provider IP :3
        .path = "/redeem_tokens",
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = _http_event_handler,
        .user_data = responseBuffer,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Attach the header to our post_data.
    memcpy(post_data, json_prefix, prefix_len);
    cur_post_length += prefix_len;

    // Iterate through the tokens and append them all into one JSON string.
    for (int i = 0; i < num_stored_tokens; i++) {
        // Slap the next token onto the string.
        token_len = token_starts[i+1] - token_starts[i];
        
        // Check to make sure if it's too big.
        if (cur_post_length + token_len >= sizeof(post_data) + suffix_len + 1) {
            printf("YOU HAVE TO MANY TOKENS!!!");
            return;
            // In the future, we would probably just want to send what we can
            // and then do some more sending later.
        }
        
        memcpy(post_data + cur_post_length, token_starts[i], token_len);
        cur_post_length += token_len;
        memcpy(post_data + cur_post_length, json_delim, delim_len);
        cur_post_length += delim_len;
    }
    
    // Attack the suffix to our post data.
    memcpy(post_data + cur_post_length - delim_len, json_suffix, suffix_len);
    //post_data[prefix_len + cur_post_length + suffix_len] = 0; // NULL terminate this so we can use strlen() later.
    int post_data_len = cur_post_length - delim_len + suffix_len;
    printf("POST request = %s\n", post_data);
    
    // POST
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, post_data_len);
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        printf("HTTP POST Status = %d, content_length = %"PRIu64"\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        printf("HTTP POST data = %s\n", responseBuffer);
        
        // Clear our token buffer.
        num_stored_tokens = 0;
    } else {
        printf("HTTP POST request failed: %s\n", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

void app_main() {

    print_chip_info();
    init_nvs();
    init_wifi();

    //
    // ~~~~~~~~ NIMBLE SCARY LAND >:( ~~~~~~~~~
    //

    // Initialize the NimBLE host configuration.
    int ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(tag, "Failed to initialize NimBLE with return code %d ", ret);
        return;
    }
    printf("NimBLE initialized\n");

    // Configure NimBLE host parameters
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    printf("BLE host configured\n");

    //Init GATT and device name 
    int rc;

    rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    if (rc != 0) {
        ESP_LOGE(tag, "Error initializing GATT server");
        return;
    }

    rc = ble_svc_gap_device_name_set("Lab11 Mule");
    if (rc != 0) {
        ESP_LOGE(tag, "Error setting device name");
        return;
    }

    printf("Peer configured and device name set\n");

    ble_store_config_init();

    //Start the muling task 
    //nimble_port_freertos_init(mule_host_task);
    //printf("Started connection\n");
    
    //
    // ~~~~~~~~ END ~~~~~~~~~
    //

    token_starts[0] = token_buff; // initialize the token storage to start at the token buffer.

    printf("Hello! Doing a quick HTTP GET test :3\n");

    http_get_test();
    
    printf("Finished our HTTP GET test!\n\nNow starting our fake data upload test! Doing a single upload for now..\n");
    
    // Make some fake data hehe.
    char fake_data[] = "hehehe this is fake data hehehe";
    http_attempt_single_upload(fake_data, strlen(fake_data));
    
    printf("Finished our single upload test!!\n\nNow trying to do multiple uploads!\n");
    
    char fake_data_2[] = "hohoho this is ~more~ fake data!";
    char *fake_datas[3];
    fake_datas[0] = fake_data;
    fake_datas[1] = fake_data_2;
    http_attempt_many_uploads(fake_datas, 2);
    
    printf("Finished our multiple upload test!!\n\nNow trying the token redemption!\n");

    /* Changed http_attempt_redeem() to read from a global variable.
    char *dummy_tokens[] = {
        "aaaaghghghghghghghghgh",
        "hehehehehehehehaasodfiho",
        "p sure tokens don't look like this but",
        "not really sure what else to put here"
    };
    http_attempt_redeem(dummy_tokens, 4);
    */
    http_attempt_redeem();
    
    printf("Finished the token redemption test! Hopefully everything works >.>\n");

    for (int i = 30; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
