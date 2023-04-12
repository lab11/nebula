/*
 * Galaxy test app with BLE and Crypto enabled
 */

#include <stdbool.h>
#include <stdint.h>
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

// Intervals for advertising and connections
static simple_ble_config_t ble_config = {
        // c0:98:e5:45:xx:xx
        .platform_id       = 0x42,    // used as 4th octect in device BLE address
        .device_id         = 0xAABB,
        .adv_name          = "SENSOR_LAB11", // used in advertisements if there is room
        .adv_interval      = MSEC_TO_UNITS(1000, UNIT_0_625_MS),
        .min_conn_interval = MSEC_TO_UNITS(500, UNIT_1_25_MS),
        .max_conn_interval = MSEC_TO_UNITS(1000, UNIT_1_25_MS),
};

//Set up BLE service and characteristic for connection with ESP 
// static simple_ble_service_t sensor_service = {{
//     .uuid128 = {0x70,0x6C,0x98,0x41,0xCE,0x43,0x14,0xA9,
//                 0xB5,0x4D,0x22,0x2B,0x89,0x10,0xE6,0x32}
// }};
static simple_ble_service_t sensor_service = {.uuid128 = {0xF3,0xAB}};

static simple_ble_char_t sensor_state_char = {.uuid16 = 0xF2AB};
static char sensor_state[25]; //TODO: fix this

simple_ble_app_t* simple_ble_app;

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

// // Function to send data over BLE
int ble_write(const unsigned char *buf, size_t len)
{
    // Check if BLE connection handle is valid
    if (simple_ble_app->conn_handle == BLE_CONN_HANDLE_INVALID) {
        printf("BLE connection handle is invalid!");
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }

    // Check connection status 
    int ret_code;
    ret_code = ble_conn_state_status(simple_ble_app->conn_handle);
    printf("Connection status: %d", ret_code);
    while (ret_code == NRF_ERROR_BUSY) {
        printf("Connection status: %d", ret_code);
        nrf_delay_ms(1000);
        ret_code = ble_conn_state_status(simple_ble_app->conn_handle);
    }

    printf("data length: %d\n", len);
    if (len > 25) {
        printf("BLE write too long! Send first chunk.");
        ble_gattc_write_params_t write_params;
        memset(&write_params, 0, sizeof(ble_gattc_write_params_t));

        unsigned char chunk[25];
        memcpy(chunk, buf, 25);

        //print data length
        printf("data length: %d\n", sizeof(chunk));

        write_params.write_op = BLE_GATT_OP_WRITE_REQ; // Use the appropriate write operation (e.g., BLE_GATT_OP_WRITE_REQ, BLE_GATT_OP_WRITE_CMD)
        write_params.handle = simple_ble_app->conn_handle; // Handle of the characteristic to write to
        write_params.p_value = chunk; // Pointer to the data to write TODO: fix to buff
        write_params.len = sizeof(chunk); // Length of the data to write

        // Use nRF5 SDK API to send data over BLE
        ret_code_t err_code = sd_ble_gattc_write(simple_ble_app->conn_handle, &write_params);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_ERROR("Failed to write BLE data, error code: %d", err_code);
            return MBEDTLS_ERR_SSL_WANT_WRITE; // Or appropriate error code for mbedtls
        }

        printf("wrote to ble! bytes: %d\n",sizeof(chunk));

        return sizeof(chunk);
    }

    else {
        ble_gattc_write_params_t write_params;
        memset(&write_params, 0, sizeof(ble_gattc_write_params_t));

        write_params.write_op = BLE_GATT_OP_WRITE_REQ; // Use the appropriate write operation (e.g., BLE_GATT_OP_WRITE_REQ, BLE_GATT_OP_WRITE_CMD)
        write_params.handle = simple_ble_app->conn_handle; // Handle of the characteristic to write to
        write_params.p_value = buf; // Pointer to the data to write TODO: fix to buff
        write_params.len = len; // Length of the data to write

        // Use nRF5 SDK API to send data over BLE
        ret_code_t err_code = sd_ble_gattc_write(simple_ble_app->conn_handle, &write_params);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_ERROR("Failed to write BLE data, error code: %d", err_code);
            return MBEDTLS_ERR_SSL_WANT_WRITE; // Or appropriate error code for mbedtls
        }

        printf("wrote to ble! bytes: %d\n",len);

        return len;
    }



}

// Function to receive data over BLE
int ble_read(unsigned char *buf, size_t len)
{
    // Cast the context to the appropriate data type (if needed)
    // e.g., ble_gap_conn_ctx_t *conn_ctx = (ble_gap_conn_ctx_t *)ctx;

    // Check if BLE connection handle is valid
    if (simple_ble_app->conn_handle == BLE_CONN_HANDLE_INVALID) {
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }

    // Use nRF5 SDK API to receive data over BLE
    // Note: You would need to implement your own logic to read data from BLE and populate the 'buf' buffer
    size_t read_len = 0;
    // Read data from BLE into 'buf' buffer and update 'read_len' accordingly
    //TODO: update buf with data from ble

    return read_len;
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
 
    simple_ble_add_characteristic(1, 1, 0, 0,
        sizeof(sensor_state), (char*)&sensor_state,
        &sensor_service, &sensor_state_char);

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
    

    printf("main loop starting\n");

    printf("sensor_svc_uuid: %x\n", sensor_service.uuid128);
    printf("sensor_state_char_uuid: %x\n", sensor_state_char.uuid16);

    // Enter main loop.
    int loop_counter = 0;
    while (loop_counter < 10) {
        nrf_gpio_pin_toggle(LED);
        nrf_delay_ms(1000);
        printf("beep!\n");
        //Send data packets while connected
        char data = "a data packet!";
        ble_write(data, sizeof(data));
        loop_counter++;
    }

    printf("done sending data, closing connection\n");

    // Cleanup 
    // printf("clean up!\n");
    // mbedtls_ecdh_free(&ctx_sensor);
    // //mbedtls_ecdh_free(&ctx_mule);
    // mbedtls_ctr_drbg_free(&ctr_drbg);
    // mbedtls_entropy_free(&entropy);
}


