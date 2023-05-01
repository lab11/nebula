/*
 * Nebula sensor test application
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
#include "aes_gcm.h"

// Definitions
#define LED NRF_GPIO_PIN_MAP(0,13)
#define CHUNK_SIZE 495
#define READ_TIMEOUT_MS 200000   /* 10 seconds */
#define READ_BUF_SIZE 4096
#define DEBUG_LEVEL 0

#define MBEDTLS_SSL_HS_TIMEOUT_MIN 200000
#define MBEDTLS_SSL_HS_TIMEOUT_MAX 400000

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
    uint32_t len;
    uint8_t total_chunks; 
};

// Sensor and Mule characteristic states
uint8_t sensor_state [CHUNK_SIZE+sizeof(struct ble_header)];
uint8_t mule_state [CHUNK_SIZE+sizeof(struct ble_header)];

//Global Sensor State
uint8_t data_buf[READ_BUF_SIZE];
uint32_t data_buf_len = 0;
uint8_t data_buf_num_chunks = 0;
uint8_t trx_state = 0;  // 0-listening,1-writing,2-recieving
uint8_t fin_buf[READ_BUF_SIZE*2]; // stores data when transfer is finished
uint32_t fin_buf_len = 0;
uint8_t chunk_ack = 0; // number of chunks we have gotten an ack for so far

//AES-GSM nounce 
uint8_t nounce[NRF_CRYPTO_AES_NOUCE_SIZE] = {0};


//Testing latency size array in kbytes
#define NUM_TEST_SIZES 10
uint32_t data_size_array[10] = {1, 2, 4}; // 16, 32, 64, 128, 256, 512};
float latency_array[10];
unsigned char buf[4*1024];


//Debugging? Enable this.
bool nebula_debug = false;

///////////////////////////////////////////////////////////////////////////////////////////////////

/* nRF rtt log init function */
int logging_init() {
    ret_code_t error_code = NRF_SUCCESS;
    error_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(error_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();
    return error_code;
}

/* setup entropy source for mebed */
void entropy_source(void *data, unsigned char *output, size_t len, size_t *olen)
{
    //Call TRNG peripheral:
    nrf_drv_rng_block_rand(output, len);
    *olen = len;

    // Return 0 on success
    //return 0;
}

/* event called when sensor gets a write over BLE*/
void ble_evt_write(ble_evt_t const * p_ble_evt) { 

    // Check if the event if on the link for this central
    if (p_ble_evt->evt.gatts_evt.conn_handle != simple_ble_app->conn_handle) {
        return;
    }

    //create a local buffer to store data
    char local_buf[CHUNK_SIZE+sizeof(struct ble_header)];

    //Check if data is mule or data and store it
    if (p_ble_evt->evt.gatts_evt.params.write.handle == mule_state_char.char_handle.value_handle) {

        //if debugging, print 
        if (nebula_debug) {
            printf("Data from Mule recieved!\n");
            printf("Data length: %d\n", p_ble_evt->evt.gatts_evt.params.write.len);
        }
       
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
            
            trx_state = 2; // switch to recieving state and recieve payload 
            memcpy(&data_buf[data_buf_len], local_buf+sizeof(struct ble_header), header->len);
            //update data_buf_len and num chunks with updated state
            data_buf_len += header->len;
            data_buf_num_chunks += 1;

            //if debugging, print updated state info
            if (nebula_debug) {
                printf("data_buf_len right after recieve: %d\n", data_buf_len);
                printf("chunk number right after recieve: %d\n", header->chunk);
                printf("data_buf_num_chunks right after recieve: %d\n", data_buf_num_chunks);
            }

            //send ack to sensor since we sucessfully got the payload
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

            //recieving an ack from sensor! 

            //if debugging, print ack info
            if (nebula_debug) {
                printf("Got an ack for chunk %d\n", header->chunk);
            }

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
            
            // put the buffer in fin_buf and reset data_buf 
            memcpy(fin_buf, data_buf, data_buf_len);
            fin_buf_len = data_buf_len;
            data_buf_len = 0;
            data_buf_num_chunks = 0;

            //if debugging, print fin info
            if (nebula_debug) {
                printf("Got a fin from mule\n");
                printf("data_buf_len: %d\n", data_buf_len);
                printf("fin_buf_len: %d\n", fin_buf_len);
            }

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
        printf("ignoring...\n");
    }
}

/* writes big packets over BLE in chunks, requires ack from sensor */
int ble_write_long(void *p_ble_conn_handle, const unsigned char *buf, size_t len) 
{
    //if debugging, print write info
    if (nebula_debug) {
        printf("sensor called ble write long\n");
    }

    int error_code = 0;
    int original_len = len;

    //check we're in a connection
    if (simple_ble_app->conn_handle == BLE_CONN_HANDLE_INVALID) {
        printf("not connected can't write\n");
        return 0;
    }

    //sanity check we are in listening state
    while (trx_state != 0) {

        //if debugging, print and delay so print is visible 
        if (nebula_debug) {
            printf("we are recieving data, wait before writing\n");
            nrf_delay_ms(1000);
        }

        //otherwise, just short delay and check again
        nrf_delay_ms(10);
    }

    //set to writing state and set chunks to 0
    trx_state = 1;
    chunk_ack = 0;

    // send all the packets over BLE
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

        //if debug, print header information
        if (nebula_debug) {
            printf("write header i: %d\n", i);
            printf("type: %d\n", header->type);
            printf("chunk: %d\n", header->chunk);
            printf("len: %d\n", header->len);
            printf("total_chunks: %d\n", header->total_chunks);
        }

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
            
            //if debugging, print and delay so print is visible
            if (nebula_debug) {
                printf("waiting for ack from mule\n");
                nrf_delay_ms(1000);
            }

            //otherwise, just short delay and check again
            nrf_delay_ms(10);

        }

    }

    //send fin to mule 
    struct ble_header *fin_header = (struct ble_header *) local_buf;
    fin_header->type = 0x02;
    fin_header->chunk = 0x00;
    fin_header->len = 0x00;
    fin_header->total_chunks = 0x00;

    //if debug, print fin message
    if (nebula_debug) {
        printf("write fin to mule\n");
    }
    
    //write fin to mule
    error_code = ble_write((char *)local_buf, sizeof(struct ble_header), &sensor_state_char, 0);

    //write is done, reset to listening
    trx_state = 0; 

    return original_len;
}

/* reads big packets over BLE in chunks */
    // * \returns        \c 0 if the connection has been closed.
    // * \returns        If performing non-blocking I/O, \c MBEDTLS_ERR_SSL_WANT_READ
    // *                 must be returned when the operation would block.
    // * \returns        Another negative error code on other kinds of failures.
int ble_read_long(void *p_ble_conn_handle, unsigned char *buf, size_t len) {

    //if debugging, print read info
    if (nebula_debug) {
        printf("sensor called ble read long\n");
    }

    // connection has been closed
    if (simple_ble_app->conn_handle == BLE_CONN_HANDLE_INVALID) {
        printf("ble_read_long: not connected can't read\n");
        return 0;
    }

    // best happy case: we have data that's finished and available to copy
    if (trx_state == 0 && fin_buf_len > 0) { // we're in listening mode

        //get copy len (either len or fin_buf_len, whichever is smaller) and copy data
        size_t copy_len = len > fin_buf_len ? fin_buf_len : len;
        memcpy(buf, fin_buf, copy_len);
        
        //need?
        //fflush(stdout);

        //clear data_buf state 
        fin_buf_len = 0;

        //if debugging, print the buf contents
        if (nebula_debug) {
            printf("\n\n\n READ BUFFER CONTENTS (%d):\n", copy_len);
            for (int i = 0; i < copy_len; i++) {
                printf("%02x ", buf[i]);
                nrf_delay_ms(10);
                fflush(stdout);
            }
            printf("\n\n\n");
        }

        //return the length of the data read
        return copy_len;
    }

    //if debug, print that we're waiting for data
    if (nebula_debug) {
        printf("ble_read_long: no data available right now, returning SSL_WANT_READ\n");
    }

    // no data available right now, return SSL_WANT_READ handshake will be called again
    return MBEDTLS_ERR_SSL_WANT_READ;
}

/* Function to send data over BLE */
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

    ret_code = sd_ble_gatts_hvx(simple_ble_app->conn_handle, &hvx_params);
    while (ret_code != NRF_SUCCESS) {

        //if debug, print error and delay so print is visible
        if (nebula_debug) {
            printf("Error writing try again: %d\n", ret_code);
            nrf_delay_ms(1000);
        }
        
        nrf_delay_ms(10);
        ret_code = sd_ble_gatts_hvx(simple_ble_app->conn_handle, &hvx_params);
    }

    return ret_code;
}

/* Function to receive data over BLE */ //TODO: remove this.
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

/* Simple read and write test */
void data_test(uint16_t ble_conn_handle) {

    printf("read and write data testing\n");
    int error_code;
    uint8_t test_buf [1000];
    uint8_t test_back [1000];

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

/* mbedtls debug function */
static void my_debug(void *ctx, int level,
                     const char *file, int line,
                     const char *str) {
    ((void) level);

    mbedtls_fprintf((FILE *) ctx, "%s:%04d: %s", file, line, str);
    fflush((FILE *) ctx);
}

/* dTLS timer functions */
struct dtls_delay_ctx {
    uint32_t int_ms;
    uint32_t fin_ms;
    bool int_timer_expired;
    bool fin_timer_expired;
};
static struct dtls_delay_ctx delay_ctx;

static void dtls_int_timer_handler(void * p_context) {
    struct dtls_delay_ctx *ctx = (struct dtls_delay_ctx *) p_context;
    printf("int timer expired\n");
    ctx->int_timer_expired = true;
}

static void dtls_fin_timer_handler(void * p_context) {
    struct dtls_delay_ctx *ctx = (struct dtls_delay_ctx *) p_context;
    printf("fin timer expired\n");
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


/* The core of it all! */
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

    const unsigned char *cert_data = nebula_srv_crt_ec; 
    ret = mbedtls_x509_crt_parse(&srvcert, (const unsigned char *) cert_data,
                                 strlen(nebula_srv_crt_ec)+1);
    if (ret != 0) {
        printf("failed\n  !  mbedtls_x509_crt_parse returned %d\n\n", ret);
       
    }

    const unsigned char *cas_pem = nebula_ca_crt;
    ret = mbedtls_x509_crt_parse(&srvcert, (const unsigned char *) cas_pem,
                                 strlen(nebula_ca_crt)+1);
    if (ret != 0) {
        printf(" failed\n  !  mbedtls_x509_crt_parse returned %d\n\n", ret);
        
    }

    const unsigned char *key_data = nebula_srv_key_ec;
    ret =  mbedtls_pk_parse_key(&pkey,
                                (const unsigned char *) key_data,
                                strlen(nebula_srv_key_ec)+1,
                                NULL,
                                0,
                                mbedtls_ctr_drbg_random,
                                &ctr_drbg);
    if (ret != 0) {
        printf(" failed\n  !  mbedtls_pk_parse_key returned %d\n\n", ret);
        
    }

    printf(" ok\n");

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

    printf("Done setting up the DTLS data...\n");
    fflush(stdout);

    //setup debug
    mbedtls_debug_set_threshold(DEBUG_LEVEL);
    mbedtls_ssl_conf_dbg(&conf, my_debug, stdout);
    //more setup...
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_read_timeout(&conf, READ_TIMEOUT_MS);
    printf("ssl conf ca chain...\n");
    fflush(stdout);
    mbedtls_ssl_conf_ca_chain(&conf, srvcert.next, NULL);
    printf("ssl conf own cert...\n");
    fflush(stdout);
    if ((ret = mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey)) != 0) {
        printf(" failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n\n", ret);
        
    }

    printf("ssl cookie setup...\n");
    fflush(stdout);
    if ((ret = mbedtls_ssl_cookie_setup(&cookie_ctx,
                                        mbedtls_ctr_drbg_random, &ctr_drbg)) != 0) {
        printf(" failed\n  ! mbedtls_ssl_cookie_setup returned %d\n\n", ret);
        
    }

    // TODO: consider adding cookies back for verify...
    // printf("ssl conf dtls cookies...\n");
    // fflush(stdout);
    // mbedtls_ssl_conf_dtls_cookies(&conf, mbedtls_ssl_cookie_write, mbedtls_ssl_cookie_check,
    //                               &cookie_ctx);

    printf("ssl setup...\n");
    fflush(stdout);
    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
        printf(" failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret);
        
    }

    printf("ssl timer cb...\n");
    fflush(stdout);

    mbedtls_ssl_set_timer_cb(&ssl, &delay_ctx, dtls_set_delay, dtls_get_delay);

    printf(" ok\n");

    //reset: TODO??

    mbedtls_ssl_session_reset(&ssl);

    
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

    printf("BLE connected...\n");
    nrf_delay_ms(1000);

    /*
    * MBEDTLS handshake
    */
    
    //Set bio to call ble connection TODO: 
    mbedtls_ssl_set_bio(&ssl, ble_conn_handle, ble_write_long, ble_read_long, NULL);

    mbedtls_ssl_conf_handshake_timeout(&conf, MBEDTLS_SSL_HS_TIMEOUT_MIN,
                                    MBEDTLS_SSL_HS_TIMEOUT_MAX);

    //start a timer for the handshake
    uint32_t start = app_timer_cnt_get();

    // handshake
    printf("trying the handshake...\n");
    ret = mbedtls_ssl_handshake(&ssl);
    while (ret != 0) {

        //if debug, print and delay
        if (nebula_debug) {
            printf("mbedtls_handshake returning %x... trying again\n", ret);
            nrf_delay_ms(1000);
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            printf("SSL/TLS handshake error: %s\n", error_buf);
            fflush(stdout);
        }

        //try again
        ret = mbedtls_ssl_handshake(&ssl);
        nrf_delay_ms(10);
    }

    //stop the timer
    uint32_t end = app_timer_cnt_get();
    uint32_t hs_time = app_timer_cnt_diff_compute(end, start);
    printf("mbedtls handshake successful, seconds: %f\n", (hs_time / (float)APP_TIMER_CLOCK_FREQ));

/*
     * 6. Read the echo Request
     */
    printf("  < Read from client:");
    fflush(stdout);

    len = sizeof(buf) - 1;
    memset(buf, 0, sizeof(buf));

    do {
        ret = mbedtls_ssl_read(&ssl, buf, len);
    } while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
             ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (ret <= 0) {
        switch (ret) {
            case MBEDTLS_ERR_SSL_TIMEOUT:
                printf(" timeout\n\n");
                //goto reset;

            case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
                printf(" connection was closed gracefully\n");
                ret = 0;
                //goto close_notify;

            default:
                printf(" mbedtls_ssl_read returned -0x%x\n\n", (unsigned int) -ret);
                //goto reset;
        }
    }

    len = ret;
    printf(" %d bytes read\n\n%s\n\n", len, buf);



    /* encrypt the data packet to write  */
    //000102030405060708090A0B0C0D0E0F08090A0B0C0D0E0F08090A0B0C0D0E0F
    uint8_t key[NRF_CRYPTO_AES_KEY_SIZE] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                                            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                                            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}; // TODO: remove this
    // uint8_t nounce[NRF_CRYPTO_AES_NOUCE_SIZE] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    //                                       0xA8, 0xA9, 0xAA, 0xAB};

    uint8_t plaintext[] = "Your message here!";
    size_t length = sizeof(plaintext) - 1; // Subtract 1 to ignore the null-terminator
    size_t payload_length = NRF_CRYPTO_AES_NOUCE_SIZE + length + NRF_CRYPTO_AES_TAG_SIZE;
    uint8_t payload[payload_length];

    printf("before encrypting...\n");

       // Print the payload
    printf("Unencrypted payload: ");
    for (size_t i = 0; i < length; i++)
    {
        printf("%02x", plaintext[i]);
    }
    printf("\n");

    // Encrypt the character array
    encrypt_character_array(key, nounce, plaintext, payload, length);

    // Print the encrypted payload
    printf("Encrypted payload: ");
    for (size_t i = 0; i < payload_length; i++)
    {
        printf("%02x", payload[i]);
    }
    printf("\n");

    //copy into buf 
    memcpy(buf, payload, payload_length);

    printf("length of buf: %d\n", sizeof(buf));
    printf("len: %d\n", len);
    printf("length of payload: %d\n", payload_length);

    len = payload_length;

    //TODO: encrypt all the payloads 


    /*
     * 7. Write the 200 Response
     */
    // for (int i = 0; i < NUM_TEST_SIZES; i++) {
    //     //setup the buffer and len to write 
    //     len = data_size_array[i]*1024;
    //     memset(buf, 'a', len);

    //     printf("  > Write to client:");

    //     //start a timer for the write
    //     start = app_timer_cnt_get();

    //     do {
    //         ret = mbedtls_ssl_write(&ssl, buf, len);
    //     } while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
    //             ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    //     if (ret < 0) {
    //         printf(" failed\n  ! mbedtls_ssl_write returned %d\n\n", ret);
    //         //goto exit;
    //     }

    //     len = ret;
    //     //printf(" %d bytes written\n\n%s\n\n", len, buf);

    //     //end timer
    //     end = app_timer_cnt_get();

    //     //print the time
    //     uint32_t write_time = app_timer_cnt_diff_compute(end, start);
    //     printf("mbedtls write successful, seconds: %f\n", (write_time / (float)APP_TIMER_CLOCK_FREQ));

    //     //put the time in the array 
    //     latency_array[i] = write_time/(float)APP_TIMER_CLOCK_FREQ;

    // }


    // normal write 
    printf("  > Write to client:");
    fflush(stdout);

    do {
        ret = mbedtls_ssl_write(&ssl, buf, len);
    } while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
             ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (ret < 0) {
        printf(" failed\n  ! mbedtls_ssl_write returned %d\n\n", ret);
        //goto exit;
    }

    len = ret;
    printf(" %d bytes written\n\n%s\n\n", len, buf);



////////////////////////////////////////////////////////////////////////////////////////



    //stop ourselves from continuing on (mbedworks!! yay)
    while(true) {
        nrf_delay_ms(1000);
        //printf("waiting after data test yay\n");
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


