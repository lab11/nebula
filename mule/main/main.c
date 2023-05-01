/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <math.h>
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
//#include "esp_crt_bundle.h" // unnecessary unless trying to run https_with_url()
#include "time.h"

#include "backhaul.h"
#include "util.h"
#include "base64.h"

struct ble_hs_adv_fields;
struct ble_gap_conn_desc;
struct ble_hs_cfg;
union ble_store_value;
union ble_store_key;

// SENSOR_LAB11
// c0:98:e5:45:aa:bb
// 0x180A

#define NEBULA_SVC_UUID 0x180A // This is the UUID for the Nebula service

static const ble_uuid_t *sensor_svc_uuid = BLE_UUID128_DECLARE(
    0x70, 0x6C, 0x98, 0x41, 0xCE, 0x43, 0x14, 0xA9,
    0xB5, 0x4D, 0x22, 0x2B, 0x89, 0x10, 0xE6, 0x32
);

static const ble_uuid_t *sensor_chr_uuid = BLE_UUID128_DECLARE(
    0x70, 0x6C, 0x98, 0x41, 0xCE, 0x43, 0x14, 0xA9,
    0xB5, 0x4D, 0x22, 0x2B, 0x11, 0x89, 0xE6, 0x32
);

static const ble_uuid_t *metadata_chr_uuid = BLE_UUID128_DECLARE(
    0x70, 0x6C, 0x98, 0x41, 0xCE, 0x43, 0x14, 0xA9,
    0xB5, 0x4D, 0x22, 0x2B, 0x12, 0x89, 0xE6, 0x32
);

#define CHUNK_SIZE 200
#define MAX_PAYLOADS 10
#define READ_TIMEOUT_MS 1000
#define MAX_RETRY       5
#define SERVER_NAME "SENSOR_LAB11"

uint8_t sensor_state [CHUNK_SIZE];
uint8_t sensor_state_data [1500]; // for storing the data 
uint8_t sensor_state_str [1500]; //for storing the certs 
uint8_t metadata_state [3];

// A big buffer and pointer array to hold our payloads :3
static int num_payloads = 0;
static uint8_t big_data [10000] = {0};
static uint8_t *payloads [MAX_PAYLOADS+1];


static const char *tag = "MULE_LAB11"; // The Mule is an ESP32 device
static int mule_ble_gap_event(struct ble_gap_event *event, void *arg);

uint16_t ble_conn_handle;
uint16_t metadata_attr_handle;
uint16_t sensor_attr_handle;

//Silly semaphore to signal when data has been written 
bool sema_metadata;
bool sema_data; 

void ble_store_config_init();

// A little buffer and pointer array to hold our tokens
static int num_stored_tokens = 0;
static char token_buff[1000] = {0}; // each token is 88 bytes
static char *token_starts[12]; // we can store up to 11 tokens

/*  Root certs for our application server and platform provider. 
    
    To embed it in the app binary, the PEM file is named
    in the component.mk COMPONENT_EMBED_TXTFILES variable in CMakeLists.txt.
*/

extern const char app_and_server_cert_pem_start[] asm("_binary_app_and_server_cert_pem_start");
extern const char app_and_server_cert_pem_end[]   asm("_binary_app_and_server_cert_pem_end");

extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");

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

    // put data into buffer depending on which characteristic was read
    if (attr->handle == metadata_attr_handle) {
        printf("Metadata recieved!\n");
        memcpy(metadata_state, attr->om->om_data, attr->om->om_len);
        sema_metadata = 1;
    } else if (attr->handle == sensor_attr_handle) {
        printf("Data recieved!\n");
        memcpy(sensor_state, attr->om->om_data, attr->om->om_len);
        sema_data = 1;
    }

    return 0; //TODO: should it sometimes return an error?
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
* App call back for subscribe to data characteristic has completed
*/
static int ble_on_subscribe(uint16_t conn_handle, const struct ble_gatt_error *error,
                            struct ble_gatt_attr *attr, void *arg) {

    MODLOG_DFLT(INFO, "Subscribe data complete; status=%d conn_handle=%d attr_handle=%d\n",
                error->status, conn_handle, attr->handle);

    // write out MTU size to console 
    MODLOG_DFLT(INFO, "MTU size: %d\n", ble_att_mtu(conn_handle));
    return 0;
}


/*
* App call back for subscribe to metadata characteristic has completed
*/
static int ble_on_subscribe_meta(uint16_t conn_handle, const struct ble_gatt_error *error,
                            struct ble_gatt_attr *attr, void *arg) {

    MODLOG_DFLT(INFO, "Subscribe meta complete; status=%d conn_handle=%d attr_handle=%d\n",
                error->status, conn_handle, attr->handle);
    return 0;
}

static void ble_read(const struct peer *peer, struct peer_chr *chr) {   
    //const struct peer_chr *chr;
    int rc;

    /* Find the UUID. */

    if (chr == NULL) {
        printf("Error: Peer doesn't support NEBULA\n");
    }

    /* Read the characteristic. */
    rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle,
                        ble_on_read, NULL);
    if (rc != 0) {
        printf("Error: Failed to read characteristic; rc=%d\n", rc);
    }
}

static void ble_write(const struct peer *peer, uint8_t *buf, const struct peer_chr *chr, size_t len) {
    //const struct peer_chr *chr;
    int rc;

    printf("in ble_write\n");

    /* Write the characteristic. */
    rc = ble_gattc_write_flat(peer->conn_handle, chr->chr.val_handle,
                              buf, len, ble_on_write, NULL);
    if (rc != 0) {
        printf("Error: Failed to write characteristic; rc=%d\n", rc);
    }
}

static void ble_subscribe(const struct peer *peer) {

    //const struct peer_chr *chr;
    const struct peer_dsc *dsc;
    const struct peer_dsc *dsc_meta;
    uint8_t value[2];
    uint8_t value_meta[2];
    int rc;

    /* Find the UUID. */
    dsc = peer_dsc_find_uuid(peer, sensor_svc_uuid, sensor_chr_uuid,
                            BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
    if (dsc == NULL) {
        printf("Error: Peer doesn't support NEBULA\n");
    }

    /* Find the metadata UUID */
    dsc_meta = peer_dsc_find_uuid(peer, sensor_svc_uuid, metadata_chr_uuid,
                            BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));

    if (dsc_meta == NULL) {
        printf("Error: Peer doesn't support NEBULA metadata\n");
    }

    sensor_attr_handle = dsc->dsc.handle;
    metadata_attr_handle = dsc_meta->dsc.handle;

    /* Subscribe to the characteristics. */
    value[0] = 1;
    value[1] = 0;
    rc = ble_gattc_write_flat(peer->conn_handle, dsc->dsc.handle,
                              value, sizeof(value), ble_on_subscribe, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to subscribe to characteristic; "
                           "rc=%d\n", rc);
    }

    printf("subscribing to metadata\n");
    //delay 1 second
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    value_meta[0] = 1;
    value_meta[1] = 0;
    rc = ble_gattc_write_flat(peer->conn_handle, dsc_meta->dsc.handle,
                              value_meta, sizeof(value_meta), ble_on_subscribe_meta, NULL);

    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to subscribe to meta characteristic; "
                           "rc=%d\n", rc);
    }

    return;

}

int ble_write_long(void *p_ble_conn_handle, const unsigned char *buf, size_t len)
{
    // //wait for connection to be established
    // while (ble_gap_conn_active() == 0) {
    //     //wait  
    //     printf("waiting for BLE connection\n");
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }

    //get peer from connection handle and peer chrs from uuids
    const struct peer *peer = peer_find(ble_conn_handle);
    const struct peer_chr *chr_metadata = peer_chr_find_uuid(peer, sensor_svc_uuid, metadata_chr_uuid);
    const struct peer_chr *chr_data = peer_chr_find_uuid(peer, sensor_svc_uuid, sensor_chr_uuid);

    //call ble_write to set metadata
    metadata_state[0] = ceil(len/(float)CHUNK_SIZE);
    metadata_state[1] = 0x00;
    ble_write(peer, metadata_state, chr_metadata, 2);

    //Send data packets in chunks
    int counter = 0; 
    int num_sent_packets = 0; 
    while (len >= CHUNK_SIZE) {
        int temp = counter + CHUNK_SIZE;
        ble_write(peer, &buf[counter], chr_data, CHUNK_SIZE);
        len = len - CHUNK_SIZE;
        counter = counter + CHUNK_SIZE;
        num_sent_packets += 1;

        //wait for ack to send next packet 
        while (metadata_state[1] != num_sent_packets) {
            printf("waiting for ack\n");
            printf("metadata state: %d\n", metadata_state[1]);
            // delay 1 second
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        printf("metadata state: %d\n", metadata_state[1]);

    }

    //write complete put back in listening mode
    metadata_state[0] = 0;
    metadata_state[1] = 0;
    metadata_state[2] = 0;
    ble_write(peer, metadata_state, chr_metadata, 3);

    return len;
}


int ble_read_long(void *p_ble_conn_handle, unsigned char *buf, size_t len) 
{
    // //wait for connection to be established TODO??
    // while (ble_gap_conn_active() == 0) {
    //     //wait  
    //     printf("waiting for BLE connection\n");
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }

    //get peer from connection handle and peer chrs from uuids 
    const struct peer *peer = peer_find(ble_conn_handle);
    const struct peer_chr *chr_metadata = peer_chr_find_uuid(peer, sensor_svc_uuid, metadata_chr_uuid);
    const struct peer_chr *chr_data = peer_chr_find_uuid(peer, sensor_svc_uuid, sensor_chr_uuid);

    // call ble_read to get metadata 
    ble_read(peer, chr_metadata);
    while (sema_metadata == 0) {
        //wait for callback to finish
    }
    //now the read data is in metadata_state
    int num_chunks = metadata_state[0]; 
    int num_recieved_chunks = metadata_state[1];

    //set the sema back to 0 since we are done with the metadata for now
    sema_metadata = 0;

    while (num_recieved_chunks < num_chunks) {
        //call ble_read to get the next data chunk 
        ble_read(peer, chr_data);
        while (sema_data == 0) {
            //wait for callback to finish
        }
        //now the read data is in sensor_state 
        memcpy(&buf[num_recieved_chunks*CHUNK_SIZE], sensor_state, CHUNK_SIZE);
        //set the sema back to 0 since we are done copying data 
        sema_data = 0;
    }

    //recieve the leftover data 
    ble_read(peer, chr_data);
    while (sema_data == 0) {
        //wait for callback to finish
    }
    //now the read data is in sensor_state
    memcpy(&buf[num_recieved_chunks*CHUNK_SIZE], sensor_state, len - num_recieved_chunks*CHUNK_SIZE);

    return len;
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

/**
 * Called when service discovery of the specified peer has completed.
 */
static void ble_on_disc_complete(const struct peer *peer, int status, void *arg) {
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

    /* 
     * Now subscribe to sensor data and metadata notifications
     */
    ble_subscribe(peer); 
    printf("subscribe done\n");
}

int ble_uuid_u128(const ble_uuid_t *uuid) {

    return uuid->type == BLE_UUID_TYPE_128 ? BLE_UUID128(uuid)->value : 0;
}


/**
 * Checks if the specified advertisement looks like a galaxy sensor.
**/
static int sensor_should_connect(const struct ble_gap_disc_desc *disc) {
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
        if (ble_uuid_u16(&fields.uuids16[i].u) == 0x180a) { //TODO fix this magic
            return 1;
        }
    }

    return 0;
}


/**
 * Connects to the sender of the specified advertisement of it looks
 * like a galaxy sensor.  A device is treated as a sensor if it advertises 
 * connectability and support for galaxy sensor service.
 */
static void mule_connect_if_sensor(void *disc) {
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
static int mule_ble_gap_event(struct ble_gap_event *event, void *arg) {
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
                        ble_on_disc_complete, NULL);
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
                    "attr_len=%d; value=",
                    event->notify_rx.indication ?
                    "indication" :
                    "notification",
                    event->notify_rx.conn_handle,
                    event->notify_rx.attr_handle,
                    OS_MBUF_PKTLEN(event->notify_rx.om));
        
        //print_mbuf(event->notify_rx.om);
        //printf("\n");
 
        //if data is sensor state, update sensor state buffer and metadata buffer
        if (event->notify_rx.attr_handle == metadata_attr_handle -1 ) { //literally no clue why -1
            //update metadata buffer
            os_mbuf_copydata(event->notify_rx.om,0,OS_MBUF_PKTLEN(event->notify_rx.om),metadata_state);
            sema_metadata = 1;

        }
        else if (event->notify_rx.attr_handle == sensor_attr_handle - 1) { //literally no clue why -1
            //check metadata to find out where to put the data 
            int number_of_chunks = metadata_state[0];
            int number_recieved_chunks = metadata_state[1];
            int readiness = metadata_state[2];

            printf("number recieved chunks %d\n", number_recieved_chunks);
            
            //update sensor state buffer
            printf("len of recieved data: %d\n", event->notify_rx.om->om_len);
            os_mbuf_copydata(event->notify_rx.om,0,OS_MBUF_PKTLEN(event->notify_rx.om),&sensor_state_data[number_recieved_chunks*CHUNK_SIZE]);
            
            metadata_state[1] += 1; //adding a packet to the metadata
            sema_data = 1;
            sema_metadata = 1;

            printf("writing ack to sensor%d\n", metadata_state[1]);

            //write an ack to the sensor
            struct peer *peer = peer_find(event->notify_rx.conn_handle);
            struct peer_chr *chr = peer_chr_find_uuid(peer, sensor_svc_uuid, metadata_chr_uuid);
            ble_write(peer, metadata_state, chr, 3);
        }
        else {
            printf("unknown characteristic data\n");
        }

        printf("metadata total chunks: %d\n",metadata_state[0]);
        printf("metadata chunks recieved: %d\n",metadata_state[1]);
        printf("metadata readiness: %d\n",metadata_state[2]);

        
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

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
    printf("Starting the mbedtls client stuff\n");

    int ret, len;
    //mbedtls_net_context server_fd;
    uint32_t flags;
    unsigned char buf[1024];
    const char *pers = "dtls_client";
    int retry_left = MAX_RETRY;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    //mbedtls_x509_crt cacert;
    mbedtls_timing_delay_context timer;

    /*
     * 0. Initialize the RNG and the session data
     */
    //mbedtls_net_init(&server_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    //mbedtls_x509_crt_init(&cacert);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_printf("\n  . Seeding the random number generator...");
    

    mbedtls_entropy_init(&entropy);
    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                     NULL,
                                     0)) != 0) {
        mbedtls_printf(" failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret);
        
    }

    mbedtls_printf(" ok\n");

    //skip cert load and net connect stuff

    mbedtls_printf("  . Setting up the DTLS structure...");
    

    if ((ret = mbedtls_ssl_config_defaults(&conf,
                                           MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        mbedtls_printf(" failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret);
        
    }

    //TODO: OPTIONAL is usually a bad choice for security
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_read_timeout(&conf, READ_TIMEOUT_MS);

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
        mbedtls_printf(" failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret);
        
    }

    if ((ret = mbedtls_ssl_set_hostname(&ssl, SERVER_NAME)) != 0) {
        mbedtls_printf(" failed\n  ! mbedtls_ssl_set_hostname returned %d\n\n", ret);
        
    }

    // Set bio to call ble connection
    mbedtls_ssl_set_bio(&ssl, ble_conn_handle, ble_write_long, ble_read_long, NULL);

    mbedtls_ssl_set_timer_cb(&ssl, &timer, mbedtls_timing_set_delay,
                              mbedtls_timing_get_delay);

    // //Handshake 
    // ret = mbedtls_ssl_handshake(&ssl);
    // if (ret != 0) {
    //     printf("error at line %d: mbedtls_ssl_handshake returned %d\n", __LINE__, ret);
    //     char error_buf[100];
    //     mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    //     printf("SSL/TLS handshake error: %s\n", error_buf);
    //      //abort();
    // }
    // else {
    //     printf("mbedtls handshake successful\n");
    // }

    // while(true) {
    //     //wait for data
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }

    // mbedtls_ssl_session_reset(&ssl);
    // // TODO call mbedtls_ssl_session_reset(&ssl) when new connection

    // printf("mbedtls done\n");


}

void mule_host_task(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/*************************************
**************************************
** HTTP STUFF STARTS HERE
**************************************
*************************************/


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

/*
// An attempt to verify my certificating..
static void https_with_url(void)
{
    esp_http_client_config_t config = {
        .url = "https://www.howsmyssl.com",
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(tag, "HTTPS Status = %d, content_length = %"PRIu64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(tag, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}
*/

// An attempt to verify my certificating...
static void https_with_hostname_path(void)
{
    // Instantiate a buffer to hold our resulting data.
    char responseBuffer[200] = {0}; // I think our responses are 197 bytes long, so this should fit.
    
    esp_http_client_config_t config = {
        .host = "34.27.170.95", //"www.howsmyssl.com",
        .path = "/public_params", //"/",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _http_event_handler,
        .cert_pem = app_and_server_cert_pem_start, //  howsmyssl_com_root_cert_pem_start,
        .skip_cert_common_name_check = true,
        .user_data = responseBuffer,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(tag, "HTTPS Status = %d, content_length = %"PRIu64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        printf("HTTP GET data = %s\n", responseBuffer);
    } else {
        ESP_LOGE(tag, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

char *encode_bytes_to_base64(char *dest, void *src, size_t src_len, size_t dest_len) {

    // First, verify that the output has enough space
    if (dest_len < ceil(src_len / 3.0) * 4) {
        printf("ERROR: Output buffer is too small to encode %d bytes to base64\n", src_len);
        return NULL;
    }

    // Now, encode the bytes
    return bintob64(dest, src, src_len);
}

// Takes in a NULL-terminated string as a payload.
void https_attempt_single_upload(uint8_t payload[], int payload_len) {
    char json_prefix[] = "{\"data\":\"";
    char json_suffix[] = "\"}";
    char post_data[1200] = {0};
    int prefix_len = sizeof(json_prefix) - 1;
    int suffix_len = sizeof(json_suffix) - 1;
    
    // Check to make sure the payload exists.
    if (payload_len <= 0) {
        printf("The payload is empty >:(");
        return;
    }
    
    // Instantiate a buffer to hold our resulting data.
    char responseBuffer[105] = {0}; // I think our responses are 101 bytes long, so this should fit.
    
    /* HTTP code
    // If everything looks ok, we do a POST request.
    esp_http_client_config_t config = {
        .host = "34.27.170.95", // hehe we only have one APP server :3
        .path = "/deliver",
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = _http_event_handler,
        .user_data = responseBuffer,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    */
    
    // If everything looks ok, we do a POST request over HTTPS.
    esp_http_client_config_t config = {
        .host = "34.31.110.212", // hehe we only have one APP server :3
        .path = "/deliver",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _http_event_handler,
        .cert_pem = app_and_server_cert_pem_start,
        .skip_cert_common_name_check = true,
        .user_data = responseBuffer,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    // POST
    memcpy(post_data, json_prefix, prefix_len);

    char *payload_end = encode_bytes_to_base64(
        post_data + prefix_len, payload, payload_len, sizeof(post_data) - prefix_len);
    if (payload_end == NULL) {
        printf("ERROR: Failed to encode payload to base64\n");
        return;
    }
    int encoded_payload_len = payload_end - (post_data + prefix_len);
    int post_len = prefix_len + encoded_payload_len + suffix_len;
    //printf("Payload = %s\n", payload);
    printf("Prefix length = %d, suffix length = %d, payload_len = %d, post_len = %d\n", prefix_len, suffix_len, encoded_payload_len, post_len);

    // Check to make sure the payload isn't too long.
    if (post_len >= sizeof(post_data)) {
        printf("The payload is too long >:(");
        return;
    }

    memcpy(post_data + prefix_len + encoded_payload_len, json_suffix, suffix_len);
    
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, post_len);
    
    //printf("POST request = %s\n", post_data);
    
    for (int aChar = 0; aChar < post_len; aChar++) {
        if (post_data[aChar] == 0) {
            printf("NULL");
        } else {
            printf("%c", post_data[aChar]);
        }
    } printf("\n");
    
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
void https_attempt_many_uploads() { // char *payloads[], int num_payloads) { // Modified to read from a global variable.
    for (int i = 0; i < num_payloads; i++) {
        // For each pointer, we do a single POST request.
        https_attempt_single_upload(payloads[i], payloads[i+1] - payloads[i]);
    }
}

// Takes in an array of pointers to NULL-terminated strings.
void https_attempt_redeem() { //char *tokens[], int num_tokens) { // Updated http_attempt_redeem() to read from global variables.
    char json_prefix[] = "{\"tokens\":[\"";
    char json_suffix[] = "\"]}";
    char json_delim[] = "\",\"";
    char post_data[1000] = {0};
    int prefix_len = sizeof(json_prefix) - 1;
    int suffix_len = sizeof(json_suffix) - 1;
    int delim_len = sizeof(json_delim) - 1;
    int token_len;
    int cur_post_length = 0;
    
    // Check to make sure we have tokens to redeem.
    if (num_stored_tokens <= 0) {
        printf("You have no tokens to redeem!\n");
        return;
    }

    // Instantiate a buffer to hold our resulting data.
    char responseBuffer[20] = {0}; // I think our responses are very small, so this should fit.
    
    // Initiate a POST request.
    esp_http_client_config_t config = {
        .host = "34.27.170.95", // hehe we hardcode the provider IP :3
        .path = "/redeem_tokens",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _http_event_handler,
        .cert_pem = app_and_server_cert_pem_start,
        .skip_cert_common_name_check = true,
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
    //
    // Initialization.
    //

    print_chip_info();
    init_nvs();
    // init_wifi();
    token_starts[0] = token_buff; // initialize the token storage to start at the token buffer.
    payloads[0] = big_data; // initialize the payload storage to start at the payload buffer.

    /* # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
     # # # # Commenting out BLE stuff to test WiFi stuff # # # #
     # # # # # # # # # # # # # # # # # # # # # # # # # # # # # */

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
    nimble_port_freertos_init(mule_host_task);
    printf("Started connection\n");
    
    //
    // ~~~~~~~~ END ~~~~~~~~~
    //
    
    //mbedtls handshake
    mbedtls_stuff();

    //start a timer 

    while(num_payloads < MAX_PAYLOADS) { // get data and send data to either server or back to sensor

        //waits for BLE connection to continue 
        // while (ble_gap_conn_active() == 0) {
        //     //wait  
        //     printf("waiting for BLE connection\n");
        //     vTaskDelay(1000 / portTICK_PERIOD_MS);
        // }

        //waiting for data transfer 
        if (metadata_state[2] != 2) {
            //printf("waiting for data transfer\n");
            //printf("metadata_state[2] = %d\n", metadata_state[2]);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        else {
            printf("data transfer complete\n");
            //copy to big buffer using payload pointers
            memcpy(payloads[num_payloads], sensor_state_data, CHUNK_SIZE*metadata_state[0]); 
            num_payloads++;
            payloads[num_payloads] = payloads[num_payloads-1] + CHUNK_SIZE*metadata_state[0];

            // TODO: do we have data to write to the sensor?
            // TODO: is it time to upload our data? 
            // Go back to waiting for data transfer state
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            metadata_state[0] = 0;
            metadata_state[1] = 0;
            metadata_state[2] = 0;
            ble_write_long(&ble_conn_handle, metadata_state, 3);
        }
    }
    //sanity checking data
    // for (int i = 0; i < MAX_PAYLOADS*CHUNK_SIZE; i++) {
    //     printf("data %x\n", big_data[i]);
    // }

    //TODO: disconnect and wait to send data to server
    printf("disconnect BLE\n");
    nimble_port_stop();

    //data transfer complete we can write data to server (or write back to sensor)
    //int len = 1000;
    //printf("sensor state data [0]%d\n", sensor_state_data[0]);
    //len = ble_write_long(&ble_conn_handle, sensor_state_data, len);



    //int len = 0;
    //uint8_t data_buf[1000];
    //len = ble_read_long(&ble_conn_handle, &data_buf, 1000);
    //len = ble_write_long(&ble_conn_handle, &data_buf, len);

    
    //get connection handle 
    
    /* # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
     # # # END commenting out BLE stuff to test WiFi stuff # # #
     # # # # # # # # # # # # # # # # # # # # # # # # # # # # # */

    /*
     * Transfer all the data up to the application server.
     */

    // Try to connect to WiFi.
    while (true) {
        esp_err_t error_code = init_wifi();
        if (error_code == ESP_OK) {
            break;
        }
        printf("Failed to connect to WiFi. Retrying...\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    /* # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
     # # # # # # # Commenting out WiFi test stuff  # # # # # # #
     # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

    // A quick check to see if we can do HTTPS stuff from the tutorial.
    printf("\nDoing a generic HTTPS with hostname path (from the tutorial)\n");
    https_with_hostname_path();

    // Generate some fake payloads (hopefully this works >.>).
    int NUM_FAKE_PAYLOADS = 5;
    uint8_t a_payload[12] = { 0xDE,0xAD,0xBE,0xEF,0x00,0xB1,0x6B,0x00,0xB5,0x00,0xB0,0xBA };
    for (int i = 0; i < NUM_FAKE_PAYLOADS; i++) {
        printf("memcpy from %p to %p len %d\n", a_payload, payloads[num_payloads], sizeof(a_payload));
        memcpy(payloads[num_payloads], a_payload, sizeof(a_payload)); 
        num_payloads++;
        payloads[num_payloads] = payloads[num_payloads-1] + sizeof(a_payload);
    }
    
     # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
     # # # # # # END commenting out WiFi test stuff  # # # # # #
     # # # # # # # # # # # # # # # # # # # # # # # # # # # # # */

    // Upload all of our data to the application servers and collect tokens :3
    https_attempt_many_uploads();
    
    printf("Finished our multiple upload!!\n\nNow trying the token redemption!\n");

    https_attempt_redeem();
    
    printf("Finished the token redemption test! Hopefully everything works >.>\n");

    //TODO: clean up mbedtls stuff and restart 
    
    for (int i = 30; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
