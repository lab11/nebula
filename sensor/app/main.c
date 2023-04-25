/*
 * Galaxy test app with BLE and Crypto enabled
 */

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
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

// Pin definitions
#define LED NRF_GPIO_PIN_MAP(0,13)
#define CHUNK_SIZE 200

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

//Set up BLE service and characteristic for connection with ESP 
static simple_ble_service_t sensor_service = {{
    .uuid128 = {0x70,0x6C,0x98,0x41,0xCE,0x43,0x14,0xA9,
                0xB5,0x4D,0x22,0x2B,0x89,0x10,0xE6,0x32}
}};

static simple_ble_char_t sensor_state_char = {.uuid16 = 0x8911};

uint8_t sensor_state [CHUNK_SIZE]; //largest possible packet need to send chunks for larger

//Set up BLE characteristic for metadata connection with ESP

static simple_ble_char_t metadata_state_char = {.uuid16 = 0x8912};

uint8_t metadata_state [3]; // [0] = number of chunks to send, [1] = chunks recieved

simple_ble_app_t* simple_ble_app;



// Silly semaphore to signal when callback is done 
bool sema_metadata;
bool sema_data; 

uint8_t *read_buf;

//prototype
int ble_write(uint16_t *buf, uint16_t len, simple_ble_char_t *characteristic, int offset);

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
    
    //Check if data is metadata or data and store in correct variable
    if (p_ble_evt->evt.gatts_evt.params.write.handle == metadata_state_char.char_handle.value_handle) {
        printf("Metadata recieved!\n");
        memcpy(metadata_state, p_ble_evt->evt.gatts_evt.params.write.data, p_ble_evt->evt.gatts_evt.params.write.len);
        sema_metadata = 1; // tells the main that the callback is done and data is ready
        printf("done with metadata callback\n");
    } 
    if (p_ble_evt->evt.gatts_evt.params.write.handle == sensor_state_char.char_handle.value_handle) {
        printf("Data recieved!\n");
        //check metadata to see where to store data 
        int num_chunks = metadata_state[0];
        int num_recieved_chunks = metadata_state[1];
        int readiness = metadata_state[2];

        memcpy(&read_buf[num_recieved_chunks*CHUNK_SIZE], p_ble_evt->evt.gatts_evt.params.write.data, p_ble_evt->evt.gatts_evt.params.write.len);
        //num_recieved_chunks++; //or actually write to characteristic 
        printf("done with data callback memcpy");

        //print to test data recieved 
        printf("Data recieved[244]: %x\n", p_ble_evt->evt.gatts_evt.params.write.data[244]);
        printf("data length: %d\n", p_ble_evt->evt.gatts_evt.params.write.len);
        //printf("num_recieved_chunks: %d\n", num_recieved_chunks);
        //printf("read_buf[num_recieved_chunks * 510]: %x\n", read_buf[num_recieved_chunks*510]);
        //printf("read_buf address %d\n", read_buf);

        metadata_state[1] +=1;
        int error_code = ble_write(metadata_state, 3, &metadata_state_char, 0);
        
        sema_data = 1; // tells the main that the callback is done and data is ready
    }
 
}

int ble_write_long(void *p_ble_conn_handle, const unsigned char *buf, size_t len) 
{
    int error_code = 0;
    int original_len = len;
    uint16_t len_for_write = (uint16_t)len;

    //check we're in a connection
    if (simple_ble_app->conn_handle == BLE_CONN_HANDLE_INVALID) {
        printf("not connected can't write\n");
        return -1;
    }

    //now we can read and write the metadata state 
    if (metadata_state[2] != 0x00) {
        printf("ESP32 is not ready to receive data\n");
        return -1;
    }

    //wait for metadata sema to be free 
    while (sema_metadata == 1) {
        //wait for metadata to be recieved
    }
    
    //write metadata test 
    printf("writing metadata\n");

    metadata_state[0] = ceil(len/(float)CHUNK_SIZE); //number of full packets to send
    metadata_state[1] = 0x00;
    metadata_state[2] = 0x01;
    error_code = ble_write(metadata_state, 3, &metadata_state_char, 0);

    //reset sema since we're done 
    sema_metadata = 0;

    //Now that sensor has a lock with metadata 
    //Send data packets in chunks of 510 bytes
    //TODO: change to for loop on both sensor and esp32 side
    int counter = 0;
    int num_sent_packets = 0;
    while (len_for_write >= CHUNK_SIZE) {
        int temp = counter + CHUNK_SIZE;
        error_code = ble_write(&buf[counter],CHUNK_SIZE, &sensor_state_char, 0);
        len_for_write -= CHUNK_SIZE;
        counter = counter + CHUNK_SIZE;
        num_sent_packets += 1;

        //wait for ack to send next packet
        while (metadata_state[1] != num_sent_packets) {
            printf("waiting for ack\n");
            printf("metadata state: %d\n", metadata_state[1]);
            printf("num sent packets: %d\n", num_sent_packets);
            nrf_delay_ms(500);
        }

        printf("number sent packets: %d\n", metadata_state[1]);
    }
    //Send remaining data
    //error_code = ble_write(&buf[counter], len_for_write, &sensor_state_char, 0);

    //wait for final ack 
    //error_code = ble_read(&metadata_state_char);
    while (metadata_state[1] != num_sent_packets) {
        printf("waiting for final ack\n");
        //error_code = ble_read(&metadata_state_char);
        nrf_delay_ms(500);
    }

    //set metadata state to signal that we are done sending data
    metadata_state[1] = 0;
    metadata_state[2] = 0x02;

    error_code = ble_write(metadata_state, 3, &metadata_state_char, 0);

    return len;
}

int ble_read_long(void *p_ble_conn_handle, unsigned char *buf, size_t len) 
{
    //call ble_read to get metadata
    int error_code;

    //now the read data is in metadata_state
    int num_chunks = metadata_state[0];
    int num_recieved_chunks = metadata_state[1];
    int readiness = metadata_state[2];


    read_buf = buf;

    

    while (readiness != 0x02) {
        printf("not ready to read\n");
        nrf_delay_ms(500);
    }

    while (metadata_state[1] < metadata_state[0]) {
        //printf("reading data\n");
        //printf("num_chunks: %d\n", metadata_state[0]);
        //printf("num_recieved_chunks: %d\n", metadata_state[1]);
        //printf("readiness: %d\n", metadata_state[2]);
        //printf("read_buf address in read_long %d\n", read_buf);
        printf("read_buf[244] in read_long %d\n", read_buf[244]);
        printf("buf[244] in read_long %d\n", buf[244]);
        nrf_delay_ms(1000);
    }

    //recieve the leftover data 
    //error_code = ble_read(&sensor_state_char);
    //while( sema_data == 0 ) {
        //wait for callback to finish
    //}
    //now the read data is in sensor_state
    //memcpy(buf[num_recieved_chunks*510], sensor_state, len - num_recieved_chunks*510);

    read_buf = NULL;
    return len;
 
}

// // Function to send data over BLE
int ble_write(uint16_t *buf, uint16_t len, simple_ble_char_t *characteristic, int offset)
{
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
        nrf_delay_ms(1000);
        ret_code = ble_conn_state_status(simple_ble_app->conn_handle);
    }

    printf("writing data to characteristic\n");
    printf("len: %d\n", len);

    ble_gatts_hvx_params_t hvx_params;
    memset(&hvx_params, 0, sizeof(hvx_params));
    hvx_params.handle = characteristic->char_handle.value_handle;
    hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = offset;
    hvx_params.p_len = &len;
    hvx_params.p_data = buf;

    printf("char handle: %d\n", hvx_params.handle);

    ret_code = sd_ble_gatts_hvx(simple_ble_app->conn_handle, &hvx_params);
    while (ret_code == NRF_ERROR_INVALID_STATE) {
        printf("Error writing try again\n");
        nrf_delay_ms(1000);
        ret_code = sd_ble_gatts_hvx(simple_ble_app->conn_handle, &hvx_params);
    }

    printf("len: %d\n", len);

    return ret_code;

}


// Function to receive data over BLE
int ble_read(simple_ble_char_t *characteristic)
{
    int ret_code;
    // Check if BLE connection handle is valid
    if (simple_ble_app->conn_handle == BLE_CONN_HANDLE_INVALID) {
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }

    // Check connection status 
    ret_code = ble_conn_state_status(simple_ble_app->conn_handle);
    while (ret_code == NRF_ERROR_BUSY) {
        printf("Connection status: %d", ret_code);
        nrf_delay_ms(1000);
        ret_code = ble_conn_state_status(simple_ble_app->conn_handle);
    }

    // Use nRF5 SDK API to receive data over BLE
    printf("reading data from characteristic\n");

    ret_code = sd_ble_gattc_read(simple_ble_app->conn_handle, characteristic->char_handle.value_handle, 0);
    while (ret_code != NRF_SUCCESS) {
        printf("Error reading try again\n");
        ret_code = sd_ble_gattc_read(simple_ble_app->conn_handle, characteristic->char_handle.value_handle, 0);
        nrf_delay_ms(1000);
    }
    
    return ret_code;
}

int main(void) {

    //setup error code
    ret_code_t error_code = NRF_SUCCESS;

    // Logging initialization
    error_code = logging_init();

    // Crypto initialization
    error_code = nrf_crypto_init();

    // Initilize mbedtls components
    mbedtls_net_context server_fd;
    mbedtls_ecdh_context ctx_sensor;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt clicert;
    mbedtls_pk_context pkey;
    mbedtls_timing_delay_context timer;

    size_t cli_olen;
    unsigned char secret_cli[32] = { 0 };
    //unsigned char secret_srv[32] = { 0 };
    unsigned char cli_to_srv[36], srv_to_cli[33];
    const char pers[] = "ecdh";

    // Initialize a mbedtls client
    //mbedtls_net_init(&server_fd); TODO this seems to only work on Windows/MAC/Linux?
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&clicert);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_pk_init(&pkey);

    // Initialize the RNG and entropy source for mbedtls
    mbedtls_entropy_init(&entropy);

    nrf_drv_rng_config_t rng_config = NRF_DRV_RNG_DEFAULT_CONFIG;
    error_code = nrf_drv_rng_init(&rng_config);

    error_code = mbedtls_entropy_add_source(&entropy, entropy_source, NULL,
                             MBEDTLS_ENTROPY_MAX_GATHER, MBEDTLS_ENTROPY_SOURCE_STRONG);
    if (error_code) {
        printf("error at line %d: mbedtls_entropy_add_source %d\n", __LINE__, error_code);
        abort();
    }

    // Seed the random number generator
    error_code = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                        NULL, 0);
    if (error_code) {
        printf("error at line %d: mbedtls_ctr_drbg_seed %d\n", __LINE__, error_code);
        abort();
    }

    //TODO: mbedtls_ssl_set_timer_cb
    
    /*
    * Load the certificates and private RSA key
    */
    const unsigned char *cert_data = sensor_cli_crt;
    error_code = mbedtls_x509_crt_parse(
        &clicert,
        cert_data,
        sensor_cli_crt_len);
    if (error_code) {
        printf("error at line %d: mbedtls_x509_crt_parse returned %d\n", __LINE__, error_code);
        abort();
    }

    const unsigned char *key_data = sensor_cli_key;
    error_code = mbedtls_pk_parse_key(
        &pkey,
        key_data,
        mule_srv_key_len, NULL, 0);
    if (error_code) {
        printf("error at line %d: mbedtls_pk_parse_key returned %d\n", __LINE__, error_code);
        abort();
    }

    /*
    * Setup SSL stuff
    */
    error_code = mbedtls_ssl_config_defaults(
        &conf,
        MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_DATAGRAM,
        MBEDTLS_SSL_PRESET_DEFAULT
    );
    if (error_code) {
        printf("error at line %d: mbedtls_ssl_config_defaults returned %d\n", __LINE__, error_code);
        abort();
    }

    mbedtls_ssl_conf_authmode( &conf, MBEDTLS_SSL_VERIFY_OPTIONAL ); //TODO change from verify optional
    mbedtls_ssl_conf_ca_chain( &conf, &clicert, NULL );
    mbedtls_ssl_conf_rng( &conf, mbedtls_ctr_drbg_random, &ctr_drbg );
    mbedtls_ssl_conf_read_timeout( &conf, 10000); //TODO change this to something reasonable/no magic numbers

    error_code = mbedtls_ssl_setup( &ssl, &conf);
    if (error_code) {
        printf("error at line %d: mbedtls_ssl_setup returned %d\n", __LINE__, error_code);
        abort();
    }

    error_code = mbedtls_ssl_set_hostname( &ssl, "localhost");
    if (error_code) {
        printf("error at line %d: mbedtls_ssl_set_hostname returned %d\n", __LINE__, error_code);
        abort();
    }
    
    /*
    * GPIO initialization
    */
    nrf_gpio_cfg_output(LED);

    /*
    * BLE initialization
    */
    simple_ble_app = simple_ble_init(&ble_config);

    simple_ble_add_service(&sensor_service);
 
    simple_ble_add_characteristic(1, 1, 1, 1,
        sizeof(sensor_state), (char*)&sensor_state,
        &sensor_service, &sensor_state_char);

    simple_ble_add_characteristic(1, 1, 1, 1,
        sizeof(metadata_state), (char*)&metadata_state,
        &sensor_service, &metadata_state_char);

    // Start Advertising
    advertising_start();

    //Wait for connection
    uint16_t ble_conn_handle = simple_ble_app->conn_handle;

    while (ble_conn_state_status(ble_conn_handle) != BLE_CONN_STATUS_CONNECTED) {
        printf("waiting to connect..\n");
        nrf_delay_ms(1000);
        ble_conn_handle = simple_ble_app->conn_handle;
    }

    printf("connected, start mbedtls handshake\n");

    /*
    * MBEDTLS handshake
    */

    
    /*
    //Set bio to call ble connection
    mbedtls_ssl_set_bio( &ssl, ble_conn_handle, ble_write, ble_read, NULL );
    //TODO: ble_write, ble_read move above connection?


    (void *ctx, const unsigned char *buf, size_t len)
    ble_write(ble_conn_handle, buf, len);

    // handshake
    int ret;
    do ret = mbedtls_ssl_handshake( &ssl );
    while( ret == MBEDTLS_ERR_SSL_WANT_READ ||
           ret == MBEDTLS_ERR_SSL_WANT_WRITE );

    if( ret != 0 )
    {
        mbedtls_printf( " failed\n  ! mbedtls_ssl_handshake returned -0x%x\n\n", (unsigned int) -ret );
        abort();
    }
    */

    printf("read and write data testing\n");
    uint8_t data_buf [1000];
    uint8_t data_back [1000];
    //chill state to start with
    metadata_state[0] = 0x00;
    metadata_state[1] = 0x00;
    metadata_state[2] = 0x00; 

    //make random data 1kB
    for (int i = 0; i < 1000; i++) {
        data_buf[i] = 0x01; // rand() % 256;
    }
    printf("data_back address: %d\n", data_back);
    error_code = ble_write_long(&ble_conn_handle, data_buf, 1000);
    error_code = ble_read_long(&ble_conn_handle, data_back, 1000);

    printf("data sent and received, checking for errors\n");
    for (int i = 0; i < 1000; i++) {
        if (data_buf[i] != data_back[i]) {
            printf("error\n");
            printf("data_buf[%d] = %d\n", i, data_buf[i]);
            printf("data_back[%d] = %d\n", i, data_back[i]);
            continue;
        }
    }
    

    // Enter main loop.
    printf("main loop starting\n");
    int loop_counter = 0;
    while (loop_counter < 10) {
        nrf_gpio_pin_toggle(LED);
        nrf_delay_ms(1000);
        printf("beep!\n");
        //sensor_state[0] = loop_counter;
        //error_code = ble_write(sensor_state, 1, &sensor_state_char);
        //loop_counter++;
        //error_code = ble_read(data_in, 1, &sensor_state_char);
    }

    printf("done sending data, closing connection\n");

    // Cleanup 
    // printf("clean up!\n");
    // mbedtls_ecdh_free(&ctx_sensor);
    // //mbedtls_ecdh_free(&ctx_mule);
    // mbedtls_ctr_drbg_free(&ctr_drbg);
    // mbedtls_entropy_free(&entropy);
}


