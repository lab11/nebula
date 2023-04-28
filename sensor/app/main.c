/*
 * Galaxy test app with BLE and Crypto enabled
 */

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "app_timer.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_uart.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrf_crypto.h"
#include "nrf_crypto_error.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_drv_rng.h"
#include "nrf_drv_timer.h"
#include "simple_ble.h"
#include "mbedtls/config.h"
#include "mbedtls/platform.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/error.h"
#include "mbedtls/ssl.h"
#include "mbedtls/timing.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl_cookie.h"
#include "mbedtls/sha256.h"
#include "mbedtls/x509_crt.h"
#include "ble_advertising.h"
#include "ble_conn_state.h"
#include "ble.h"
#include "certs.h"
#include "data.h"


// Pin definitions
#define LED NRF_GPIO_PIN_MAP(0,13)
#define CHUNK_SIZE 200
#define READ_TIMEOUT_MS 10000   /* 10 seconds */
#define READ_BUF_SIZE 1024
#define DEBUG_LEVEL 4

// Intervals for advertising and connections
static simple_ble_config_t ble_config = {
        // c0:98:e5:45:aa:bb
        .platform_id       = 0x42,    // used as 4th octect in device BLE address
        .device_id         = 0xAABB,
        .adv_name          = "SENSOR_LAB11", // used in advertisements if there is room
        .adv_interval      = MSEC_TO_UNITS(1000, UNIT_0_625_MS),
        .min_conn_interval = MSEC_TO_UNITS(500, UNIT_1_25_MS),
        .max_conn_interval = MSEC_TO_UNITS(1000, UNIT_1_25_MS),
};

//Set up BLE services and characteristics for connection with ESP 
static simple_ble_service_t sensor_service = {{
    .uuid128 = {0x70,0x6C,0x98,0x41,0xCE,0x43,0x14,0xA9,
                0xB5,0x4D,0x22,0x2B,0x89,0x10,0xE6,0x32}
}};

static simple_ble_char_t sensor_state_char = {.uuid16 = 0x8911};
static simple_ble_char_t mule_state_char = {.uuid16 = 0x8912};

simple_ble_app_t* simple_ble_app;

//pointer to read buffer
uint8_t *read_buf;

//dtls timers
APP_TIMER_DEF(dtls_int_timer_id);
APP_TIMER_DEF(dtls_fin_timer_id);

// Prototype functions
int ble_write(unsigned char *buf, uint16_t len, simple_ble_char_t *characteristic, int offset);


// Header struct 
struct ble_header {
    uint8_t type;
    uint8_t chunk;
    uint8_t len;
    uint8_t total_chunks; 
};

uint8_t sensor_state [CHUNK_SIZE+sizeof(struct ble_header)];
uint8_t mule_state [CHUNK_SIZE+sizeof(struct ble_header)];

//Global Sensor State
uint8_t data_buf[READ_BUF_SIZE];
uint32_t data_buf_len = 0;
uint8_t data_buf_num_chunks = 0;
uint8_t trx_state = 0;  // 0-listening,1-writing,2-recieving

uint8_t chunk_ack = 0; // number of chunks we have gotten an ack for so far

///////////////////////////////////////////////////////////////////////////////////////////////////

int logging_init() {
    ret_code_t error_code = NRF_SUCCESS;
    error_code = NRF_LOG_INIT(NULL);
    //APP_ERROR_CHECK(error_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();
    return error_code;
}

int entropy_source(void *data, unsigned char *output, size_t len, size_t *olen)
{
    //Call TRNG peripheral:
    nrf_drv_rng_block_rand(output, len);
    *olen = len;

    // Return 0 on success
    return 0;
}

void ble_evt_write(ble_evt_t const * p_ble_evt) { 
    // Check if the event if on the link for this central
    if (p_ble_evt->evt.gatts_evt.conn_handle != simple_ble_app->conn_handle) {
        return;
    }

    else {
        //create a local buffer to store data
        char local_buf[CHUNK_SIZE+sizeof(struct ble_header)];

        //Check if data is mule or data and store it
        if (p_ble_evt->evt.gatts_evt.params.write.handle == mule_state_char.char_handle.value_handle) {
            printf("Data from Mule recieved!\n");
            printf("Data length: %d\n", p_ble_evt->evt.gatts_evt.params.write.len);
            //copy recieved data into local buffer 
            memcpy(local_buf, p_ble_evt->evt.gatts_evt.params.write.data, p_ble_evt->evt.gatts_evt.params.write.len);

            //check header to see where to store data
            struct ble_header *header = (struct ble_header *) local_buf;
            if (header->type == 0x00) { // mule is sending a payload 
                //check if we were writing (bad times)
                if (trx_state == 1) {
                    printf("Mule is sending a payload while we are writing\n");
                    return;
                }

                //check we're ready to get more data 
                if (trx_state == 0 && data_buf_len != 0) {
                    printf("Mule is trying to send a new payload while our data buffer has content\n");
                    return;
                }
                
                //sanity check on chunk number
                if (header->chunk != data_buf_num_chunks) {
                    printf("Mule is sending a payload with the wrong chunk number\n");
                    return;
                }
                
                trx_state = 2; // switch to recieving state
                // recieve payload 
                memcpy(&data_buf[data_buf_len], local_buf+sizeof(struct ble_header), header->len);
                //update data_buf_len
                data_buf_len += header->len;
                data_buf_num_chunks += 1;

                //send ack 
                struct ble_header ack_header = {0x01, header->chunk, header->len, header->total_chunks}; //TODO: sanity check len of header is right. 
                ble_write((unsigned char *) &ack_header, sizeof(struct ble_header), &sensor_state_char, 0);
 
            } 
            else if (header->type == 0x01) { // mule is sending an ack
                //check if we were listening (bad times)
                if (trx_state == 0) {
                    printf("Mule is sending an ack while we are listening\n");
                    return;
                }

                //check if we were recieving (bad times)
                if (trx_state == 2) {
                    printf("Mule is sending an ack while we are recieving data\n");
                    return;
                }

                //recieving an ack! 
                printf("Got an ack for chunk %d\n", header->chunk);

                //check if ack is for the right chunk
                if (header->chunk != chunk_ack) {
                    printf("Mule is sending an ack for the wrong chunk\n");
                    return;
                }
                else { //chunk is the right chunks
                    chunk_ack += 1;
                }

            }
            else if (header->type == 0x02) { // mule is sending a fin
                // we're done go back to listening
                trx_state = 0;
            }
        } 

        else if (p_ble_evt->evt.gatts_evt.params.write.handle == sensor_state_char.char_handle.value_handle) {
            printf("Got a write to sensor characteristic!\n");
            printf("Shouldn't happen!\n");
        }

        else {
            printf("Got a write to a non-Nebula characteristic!\n");
            //printf("char handle: %d\n", p_ble_evt->evt.gatts_evt.params.write.handle);
            //printf("mule handle: %d\n", mule_state_char.char_handle.value_handle);
            //printf("sensor handle: %d\n", sensor_state_char.char_handle.value_handle);
            printf("ignoring...\n");
        }
    }
}

int ble_write_long(void *p_ble_conn_handle, const unsigned char *buf, size_t len) 
{
    printf("sensor called ble write long\n");

    int error_code = 0;
    int original_len = len;

    //check we're in a connection
    if (simple_ble_app->conn_handle == BLE_CONN_HANDLE_INVALID) {
        printf("not connected can't write\n");
        return 0;
    }

    //sanity check we are in listening state
    while (trx_state != 0) {
        printf("we are recieving data, wait before writing\n");
        nrf_delay_ms(1000);
    }

    //set to writing state and set chunks to 0
    trx_state = 1;
    chunk_ack = 0;

    printf("writing bytes\n");

    int num_packets = ceil(len/(float)CHUNK_SIZE);
    char local_buf[CHUNK_SIZE+sizeof(struct ble_header)];

    for (int i = 0; i < num_packets; i++) {

        int len_for_write = 0;

        // we have one left to go! 
        if (i == num_packets - 1) {
            // and it's the perfect size
            if (len == CHUNK_SIZE) {
                len_for_write = CHUNK_SIZE;
            }
            // and it's a leftover bit
            else{
                len_for_write = len % CHUNK_SIZE;
            }
        }
        // we have more than one, write a full chunk 
        else {
            len_for_write = CHUNK_SIZE;
        }

        //set up header
        struct ble_header *header = (struct ble_header *) local_buf;
        header->type = 0x00;
        header->chunk = i;
        header->len = len_for_write;
        header->total_chunks = num_packets;

        //print header information
        printf("write header i: %d\n", i);
        printf("type: %d\n", header->type);
        printf("chunk: %d\n", header->chunk);
        printf("len: %d\n", header->len);
        printf("total_chunks: %d\n", header->total_chunks);

        //copy data into local buffer
        memcpy(local_buf + sizeof(struct ble_header), &buf[i*CHUNK_SIZE], len_for_write); //copy data into local buffer

        //write data over ble and decrement the len 
        error_code = ble_write((unsigned char *)local_buf, len_for_write+sizeof(struct ble_header), &sensor_state_char, 0);
        if (error_code != NRF_SUCCESS) {
            printf("ble_write failed sad: %d\n", error_code);
        }
        len -= len_for_write;

        //wait for ack saying number of packets recieved is same as sent
        while (chunk_ack != i+1) {
            nrf_delay_ms(1000);
            printf("waiting for ack from mule\n");
        }

    }

    //send fin to mule 
    struct ble_header *fin_header = (struct ble_header *) local_buf;
    fin_header->type = 0x02;
    fin_header->chunk = 0x00;
    fin_header->len = 0x00;
    fin_header->total_chunks = 0x00;
    
    error_code = ble_write((char *)local_buf, sizeof(struct ble_header), &sensor_state_char, 0);

    //write is done, reset to listening
    trx_state = 0; 

    return original_len;
}

int ble_read_long(void *p_ble_conn_handle, unsigned char *buf, size_t len) {

    printf("sensor called ble read long\n");

    // If data has been received, the positive number of bytes received.
    // * \returns        \c 0 if the connection has been closed.
    // * \returns        If performing non-blocking I/O, \c MBEDTLS_ERR_SSL_WANT_READ
    // *                 must be returned when the operation would block.
    // * \returns        Another negative error code on other kinds of failures.

    // connection has been closed
    if (simple_ble_app->conn_handle == BLE_CONN_HANDLE_INVALID) {
        printf("ble_read_long: not connected can't read\n");
        return 0;
    }

    // best happy case: we have data that's finished and available to copy
    if (trx_state == 0 && data_buf_len > 0) { // we're in listening mode
        memcpy(buf, data_buf, data_buf_len);
        printf("ble_read_long: read %d bytes\n", data_buf_len);

        //clear data_buf state 
        data_buf_len = 0;
        data_buf_num_chunks = 0;

        return data_buf_len;
    }

    printf("ble_read_long: no data available\n");
    return MBEDTLS_ERR_SSL_WANT_READ;
}

// Function to send data over BLE
int ble_write(unsigned char *buf, uint16_t len, simple_ble_char_t *characteristic, int offset) {

    // Check if BLE connection handle is valid
    if (simple_ble_app->conn_handle == BLE_CONN_HANDLE_INVALID) {
        printf("BLE connection handle is invalid!\n");
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }

    // Check connection status 
    int ret_code;
    ret_code = ble_conn_state_status(simple_ble_app->conn_handle);
    while (ret_code != BLE_CONN_STATUS_CONNECTED) {
        printf("Connection status: %d", ret_code);
        nrf_delay_ms(100);
        ret_code = ble_conn_state_status(simple_ble_app->conn_handle);
    }

    ble_gatts_hvx_params_t hvx_params;
    memset(&hvx_params, 0, sizeof(hvx_params));
    hvx_params.handle = characteristic->char_handle.value_handle;
    hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = offset;
    hvx_params.p_len = &len;
    hvx_params.p_data = buf;

    printf("Writing %d bytes to handle %d\n", len, hvx_params.handle);

    ret_code = sd_ble_gatts_hvx(simple_ble_app->conn_handle, &hvx_params);
    while (ret_code != NRF_SUCCESS) {
        printf("Error writing try again: %d\n", ret_code);
        nrf_delay_ms(1000);
        ret_code = sd_ble_gatts_hvx(simple_ble_app->conn_handle, &hvx_params);
    }

    printf("Wrote %d bytes to handle %d\n", len, hvx_params.handle);

    return ret_code;

}


// Function to receive data over BLE
int ble_read(simple_ble_char_t *characteristic) {

    int ret_code;
    // Check if BLE connection handle is valid
    if (simple_ble_app->conn_handle == BLE_CONN_HANDLE_INVALID) {
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }

    // Check connection status 
    ret_code = ble_conn_state_status(simple_ble_app->conn_handle);
    while (ret_code == NRF_ERROR_BUSY) {
        printf("Connection status: %d", ret_code);
        nrf_delay_ms(100);
        ret_code = ble_conn_state_status(simple_ble_app->conn_handle);
    }

    ret_code = sd_ble_gattc_read(simple_ble_app->conn_handle, characteristic->char_handle.value_handle, 0);
    while (ret_code != NRF_SUCCESS) {
        printf("Error reading try again\n");
        ret_code = sd_ble_gattc_read(simple_ble_app->conn_handle, characteristic->char_handle.value_handle, 0);
        nrf_delay_ms(100);
    }
    
    return ret_code;
}

void data_test(uint16_t ble_conn_handle) {

    printf("read and write data testing\n");
    int error_code;
    uint8_t test_buf [1000];
    uint8_t test_back [1000];
 
    // set the header data 
    // data_buf[0] = 0x02;
    // data_buf[1] = 0x00;
    // data_buf[2] = 0x00;

    //make random data 1kB
    for (int i = 0; i < 1000; i++) {
        test_buf[i] = rand() % 256;
    }

    error_code = ble_write_long(&ble_conn_handle, (char *)test_buf, 1000);
    error_code = ble_read_long(&ble_conn_handle, (char *)test_back, 1000);

    printf("data sent and received, checking for errors\n");
    for (int i = 0; i < 1000; i++) {
        if (test_buf[i] != test_back[i]) {
            printf("error\n");
            //printf("data_buf[%d] = %d\n", i, test_buf[i]);
            //printf("data_back[%d] = %d\n", i, test_back[i]);
            continue;
        }
    }
}

static void my_debug(void *ctx, int level,
                     const char *file, int line,
                     const char *str)
{
    ((void) level);

    mbedtls_fprintf((FILE *) ctx, "%s:%04d: %s", file, line, str);
    fflush((FILE *) ctx);
}

//dTLS timer functions 
struct dtls_delay_ctx {
    uint32_t int_ms;
    uint32_t fin_ms;
    bool int_timer_expired;
    bool fin_timer_expired;
};
static struct dtls_delay_ctx delay_ctx;

static void dtls_int_timer_handler(void * p_context) {
    struct dtls_delay_ctx *ctx = (struct dtls_delay_ctx *) p_context;
    ctx->int_timer_expired = true;
}

static void dtls_fin_timer_handler(void * p_context) {
    struct dtls_delay_ctx *ctx = (struct dtls_delay_ctx *) p_context;
    ctx->fin_timer_expired = true;
}

void dtls_set_delay(void *data, uint32_t int_ms, uint32_t fin_ms) {

    struct dtls_delay_ctx *ctx = (struct dtls_delay_ctx *) data;
    ctx->int_ms = int_ms;
    ctx->fin_ms = fin_ms;
    ctx->int_timer_expired = false;
    ctx->fin_timer_expired = false;

    ret_code_t error_code = app_timer_stop_all();
    APP_ERROR_CHECK(error_code);

    // don't restart timers if we don't have a delay
    if (int_ms == 0 && fin_ms == 0) {
        return;
    }

    error_code = app_timer_start(dtls_int_timer_id, APP_TIMER_TICKS(int_ms), data);
    APP_ERROR_CHECK(error_code);

    error_code = app_timer_start(dtls_fin_timer_id, APP_TIMER_TICKS(fin_ms), data);
    APP_ERROR_CHECK(error_code);
}

int dtls_get_delay(void *data) {

    struct dtls_delay_ctx *ctx = (struct dtls_delay_ctx *) data;
    if (ctx->fin_ms == 0) {
        return -1;
    }

    if (ctx->fin_timer_expired) {
        return 2;
    }

    if (ctx->int_timer_expired) {
        return 1;
    }

    return 0; 
}

int main(void) {

    //setup error code
    ret_code_t error_code = NRF_SUCCESS;

    // Logging initialization
    error_code = logging_init();

    // Crypto initialization
    error_code = nrf_crypto_init();

    // put simple BLE up here so we can piggy-back on the app timer initialization
    simple_ble_app = simple_ble_init(&ble_config);

    // dtls timer initialization
    error_code = app_timer_create(&dtls_int_timer_id, APP_TIMER_MODE_SINGLE_SHOT, dtls_int_timer_handler);
    APP_ERROR_CHECK(error_code);

    error_code = app_timer_create(&dtls_fin_timer_id, APP_TIMER_MODE_SINGLE_SHOT, dtls_fin_timer_handler);
    APP_ERROR_CHECK(error_code);


    // mbedTLS initialization
    int ret, len;
    mbedtls_net_context server_fd;
    unsigned char buf[1024];
    const char *pers = 'dtls_server/sensor'; 
    unsigned char client_ip[16] = { 0 };
    size_t cliip_len;
    mbedtls_ssl_cookie_ctx cookie_ctx;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context pkey;

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_ssl_cookie_init(&cookie_ctx);

    mbedtls_x509_crt_init(&srvcert);
    mbedtls_pk_init(&pkey);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    /*
     * 1. Seed the RNG
     */

    //Initialize the RNG and entropy source for mbedtls
    nrf_drv_rng_config_t rng_config = NRF_DRV_RNG_DEFAULT_CONFIG;
    error_code = nrf_drv_rng_init(&rng_config);

    error_code = mbedtls_entropy_add_source(&entropy, entropy_source, NULL,
                             MBEDTLS_ENTROPY_MAX_GATHER, MBEDTLS_ENTROPY_SOURCE_STRONG);


    printf("Seeding the random number generator...\n");

    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                     NULL,
                                     0)) != 0) {
        printf("failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret);
        
    }

    printf("ok\n");

    /*
     * 2. Load the certificates and private RSA key
     */
    printf("Loading the server cert. and key...\n");

    const unsigned char *cert_data = sensor_cli_crt; //TODO: change name to sensor_svr_crt
    ret = mbedtls_x509_crt_parse(&srvcert, (const unsigned char *) cert_data,
                                 sensor_cli_crt_len);
    if (ret != 0) {
        printf("failed\n  !  mbedtls_x509_crt_parse returned %d\n\n", ret);
       
    }

    const unsigned char *key_data = sensor_cli_key;
    ret =  mbedtls_pk_parse_key(&pkey,
                                (const unsigned char *) key_data,
                                sensor_cli_key_len,
                                NULL,
                                0);
    if (ret != 0) {
        printf(" failed\n  !  mbedtls_pk_parse_key returned %d\n\n", ret);
        
    }

    printf(" ok\n");

    //TODO: I skipped setting up the listening UDP port since it's not needed in BLE dTLS

    /*
     * 4. Setup DTLS stuff
     */
    printf("Setting up the DTLS data...\n");
    fflush(stdout);

    if ((ret = mbedtls_ssl_config_defaults(&conf,
                                           MBEDTLS_SSL_IS_SERVER,
                                           MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        mbedtls_printf(" failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret);
        
    }

    //setup debug
    
    mbedtls_debug_set_threshold(DEBUG_LEVEL);
    mbedtls_ssl_conf_dbg(&conf, my_debug, stdout);

    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_read_timeout(&conf, READ_TIMEOUT_MS);
    mbedtls_ssl_conf_ca_chain(&conf, srvcert.next, NULL);
    if ((ret = mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey)) != 0) {
        printf(" failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n\n", ret);
        
    }

    if ((ret = mbedtls_ssl_cookie_setup(&cookie_ctx,
                                        mbedtls_ctr_drbg_random, &ctr_drbg)) != 0) {
        printf(" failed\n  ! mbedtls_ssl_cookie_setup returned %d\n\n", ret);
        
    }

    mbedtls_ssl_conf_dtls_cookies(&conf, mbedtls_ssl_cookie_write, mbedtls_ssl_cookie_check,
                                  &cookie_ctx);

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
        printf(" failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret);
        
    }

    mbedtls_ssl_set_timer_cb(&ssl, &delay_ctx, dtls_set_delay, dtls_get_delay);

    printf(" ok\n");

    //reset: TODO???

    mbedtls_ssl_session_reset(&ssl);

    //TODO: I skipped the wait until a client connects and client ID cause that's not needed

    
    /*
    * GPIO initialization
    */
    nrf_gpio_cfg_output(LED);

    /*
    * BLE initialization
    */

    simple_ble_add_service(&sensor_service);
 
    simple_ble_add_characteristic(1, 1, 1, 1,
        sizeof(sensor_state), (char*)&sensor_state,
        &sensor_service, &sensor_state_char);

    simple_ble_add_characteristic(1, 1, 1, 1,
        sizeof(mule_state), (char*)&mule_state,
        &sensor_service, &mule_state_char);

    // Start Advertising
    advertising_start();

    //Wait for connection
    uint16_t ble_conn_handle = simple_ble_app->conn_handle;

    while (ble_conn_state_status(ble_conn_handle) != BLE_CONN_STATUS_CONNECTED) {
        printf("waiting to connect..\n");
        nrf_delay_ms(100);
        ble_conn_handle = simple_ble_app->conn_handle;
    }

    printf("BLE connected, waiting 5 seconds...\n");
    nrf_delay_ms(5000);

    //printf("BLE connected, start mbedtls handshake\n");

    //Read and write test
    //data_test(ble_conn_handle);
    //read test 

    /*
    * MBEDTLS handshake
    */
////////////////////////////////////////////////////////////////////////////////////////
    
    //Set bio to call ble connection TODO: 
    mbedtls_ssl_set_bio(&ssl, ble_conn_handle, ble_write_long, ble_read_long, NULL);

    // handshake
    ret = mbedtls_ssl_handshake(&ssl);
    while (ret != 0) {
        //try again  
        nrf_delay_ms(5000);
        ret = mbedtls_ssl_handshake(&ssl);
    }
    // while( ret == MBEDTLS_ERR_SSL_WANT_READ ||
    //          ret == MBEDTLS_ERR_SSL_WANT_WRITE );

    if( ret != 0 )
    {
        mbedtls_printf( " failed\n  ! mbedtls_ssl_handshake returned -0x%x\n\n", (unsigned int) -ret );
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        printf("SSL/TLS handshake error: %s\n", error_buf);
        //abort();
    }
    else {
        printf("mbedtls handshake successful\n");
    }


    //TODO: actually send data encrypted over mbedtls

////////////////////////////////////////////////////////////////////////////////////////



    //stop ourselves from continuing on (mbedworks!! yay)
    while(true) {
        nrf_delay_ms(1000);
        printf("waiting after data test yay\n");
    }


    //End-to-End test
    while(true) {

        while (ble_conn_state_status(ble_conn_handle) != BLE_CONN_STATUS_CONNECTED) {
            printf("waiting to connect..\n");
            nrf_delay_ms(100);
            ble_conn_handle = simple_ble_app->conn_handle;
        }

        if (mule_state[2] == 2 ) {
            printf("waiting for mule to send data back\n");
            nrf_delay_ms(5000); // give em .5 seconds
            mule_state[0] = 0;
            mule_state[1] = 0;
            mule_state[2] = 0;
            error_code = ble_write( (char *)mule_state, 3, &mule_state_char, 0);
        }
        else if (mule_state[2] == 1) {
            //already sending data
        }
        else {
            // time to send some more data //todo make sensor state data
            printf("time to send more!\n");
            //uint8_t data_test [1000];
            error_code = ble_write_long(&ble_conn_handle, (char*)data, 1000);
            printf("made it through sending data\n");
            nrf_delay_ms(500);

        }
    }

    printf("done sending data, closing connection\n");

    // TODO: after mbedtls Cleanup 
    // printf("clean up!\n");
    // mbedtls_ecdh_free(&ctx_sensor);
    // //mbedtls_ecdh_free(&ctx_mule);
    // mbedtls_ctr_drbg_free(&ctr_drbg);
    // mbedtls_entropy_free(&entropy);
}


