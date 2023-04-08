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
#include "ble_advertising.h"
#include "ble_conn_state.h"
#include "ble.h"

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
static simple_ble_service_t sensor_service = {{
    .uuid128 = {0x70,0x6C,0x98,0x41,0xCE,0x43,0x14,0xA9,
                0xB5,0x4D,0x22,0x2B,0x89,0x10,0xE6,0x32}
}};

static simple_ble_char_t sensor_state_char = {.uuid16 = 0x8911};
static bool sensor_state = false;

simple_ble_app_t* simple_ble_app;

int logging_init() {
    ret_code_t error_code = NRF_SUCCESS;
    error_code = NRF_LOG_INIT(NULL);
    //APP_ERROR_CHECK(error_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();
    return error_code;
}

void ble_evt_write(ble_evt_t const* p_ble_evt) {
    if (simple_ble_is_char_event(p_ble_evt, &sensor_state_char)) {
      printf("Got write to LED characteristic!\n");
      if (sensor_state) {
        printf("Turning on LED!\n");
        nrf_gpio_pin_clear(LED);
      } else {
        printf("Turning off LED!\n");
        nrf_gpio_pin_set(LED);
      }
    }
}

int main(void) {

    //setup error code
    ret_code_t error_code = NRF_SUCCESS;

    // Logging initialization
    error_code = logging_init();

    // Crypto initialization
    error_code = nrf_crypto_init();
    APP_ERROR_CHECK(error_code);

    // Initilize mbedtls 
    mbedtls_net_context server_fd;
    mbedtls_ecdh_context ctx_sensor;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
    mbedtls_timing_delay_context timer;

    int exit = MBEDTLS_EXIT_FAILURE;
    //size_t srv_olen;
    size_t cli_olen;
    unsigned char secret_cli[32] = { 0 };
    //unsigned char secret_srv[32] = { 0 };
    unsigned char cli_to_srv[36], srv_to_cli[33];
    const char pers[] = "ecdh";

    // Initialize a mbedtls client
    //mbedtls_net_init(&server_fd); TODO this seems to only work on Windows/MAC/Linux?
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&cacert);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    // Initialize a mbedtls client
    //mbedtls_ecdh_init(&ctx_sensor); TODO???????
    //mbedtls_ecp_group_load(&ctx_sensor.grp, MBEDTLS_ECP_DP_CURVE25519);

    // Seed the random number generator
    mbedtls_entropy_init(&entropy);
    error_code = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                        (const unsigned char *)pers, strlen(pers));

    //error_code = mbedtls_ecdh_setup(&ctx_sensor, MBEDTLS_ECP_DP_CURVE25519);
    // look into porting to nrf sdk 16 if we need this function

    //Generate a public key and a TLS ServerKeyExchange payload.
    error_code = mbedtls_ecdh_make_params(&ctx_sensor, &cli_olen, cli_to_srv,
                                   sizeof(cli_to_srv),
                                   mbedtls_ctr_drbg_random, &ctr_drbg);


    printf("gen 1\n");
    // Generate a public key for the sensor
    error_code = mbedtls_ecdh_gen_public(
        &ctx_sensor.grp,            // Elliptic curve group
        &ctx_sensor.d,              // Sensor secret key
        &ctx_sensor.Q,              // Sensor public key
        mbedtls_ctr_drbg_random,    
        &ctr_drbg
    );
    //APP_ERROR_CHECK(error_code);

    // printf("gen 2\n");
    // // Generate a key for the mule (XXX shouldn't stay here long-term)
    // error_code = mbedtls_ecdh_gen_public(
    //     &ctx_mule.grp,            // Elliptic curve group
    //     &ctx_mule.d,              // Sensor secret key
    //     &ctx_mule.Q,              // Sensor public key
    //     mbedtls_ctr_drbg_random,    
    //     &ctr_drbg
    // );
    // APP_ERROR_CHECK(error_code);

    //read in public key from the mule
    // error_code = mbedtls_ecdh_read_public(&ctx_sensor, srv_to_cli, 
    //                                 sizeof(srv_to_cli));


    // // Compute shared secrets 
    // error_code = mbedtls_ecdh_calc_secret(&ctx_sensor, &cli_olen, secret_cli,
    //                                sizeof(secret_cli),
    //                                mbedtls_ctr_drbg_random, &ctr_drbg);

    // error_code = mbedtls_ecdh_calc_secret(&ctx_mule, &srv_olen, secret_srv,
    //                                sizeof(secret_srv),
    //                                mbedtls_ctr_drbg_random, &ctr_drbg);
    
    // GPIO initialization
    nrf_gpio_cfg_output(LED);

    //printf("mbedtls init done\n");

    // BLE initialization
    simple_ble_app = simple_ble_init(&ble_config);

    simple_ble_add_service(&sensor_service);

    //setup data packet 
    

    simple_ble_add_characteristic(1, 1, 0, 0,
        sizeof(sensor_state), (uint8_t*)&sensor_state,
        &sensor_service, &sensor_state_char);

    // Start Advertising
    //advertising_init();
    advertising_start();

    //Wait for connection
    uint16_t ble_conn_handle = simple_ble_app->conn_handle;

    while (ble_conn_state_status(ble_conn_handle) != BLE_CONN_STATUS_CONNECTED) {
        printf("waiting to connect..\n");
        ble_conn_handle = simple_ble_app->conn_handle;
    }

    printf("connected, start mbedtls handshake\n");
    

    printf("main loop starting\n");

    // Enter main loop.
    int loop_counter = 0;
    while (loop_counter < 10) {
        nrf_gpio_pin_toggle(LED);
        nrf_delay_ms(1000);
        printf("beep!\n");
    }

    // Cleanup 
    printf("clean up!\n");
    mbedtls_ecdh_free(&ctx_sensor);
    //mbedtls_ecdh_free(&ctx_mule);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}


