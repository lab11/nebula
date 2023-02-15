/*
 * Galaxy test app with BLE and Crypto enabled
 */

#include <stdbool.h>
#include <stdint.h>
#include "nrf.h"
#include "nrf_delay.h"
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

// Pin definitions
#define LED NRF_GPIO_PIN_MAP(0,13)

// Intervals for advertising and connections
static simple_ble_config_t ble_config = {
        // c0:98:e5:45:xx:xx
        .platform_id       = 0x42,    // used as 4th octect in device BLE address
        .device_id         = 0xAABB,
        .adv_name          = "TESS_LAB11", // used in advertisements if there is room
        .adv_interval      = MSEC_TO_UNITS(1000, UNIT_0_625_MS),
        .min_conn_interval = MSEC_TO_UNITS(500, UNIT_1_25_MS),
        .max_conn_interval = MSEC_TO_UNITS(1000, UNIT_1_25_MS),
};

simple_ble_app_t* simple_ble_app;

int main(void) {

    // Logging initialization
    ret_code_t error_code = NRF_SUCCESS;
    error_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(error_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    // Crypto initialization
    error_code = nrf_crypto_init();
    APP_ERROR_CHECK(error_code);

    mbedtls_ecdh_context ctx_sensor, ctx_mule;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    NRF_CRYPTOCELL->ENABLE=1;

    // for now, initialize a client AND server context. Eventually, one of
    // these contexts will move off onto the ESP
    mbedtls_ecdh_init(&ctx_sensor);
    mbedtls_ecdh_init(&ctx_mule);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    // TODO figure out how to hook up the Cryptocell as an entropy source
    //mbedtls_entropy_init(&entropy); entropy poll only seems to work on unix and windows
    
    // GPIO initialization
    nrf_gpio_cfg_output(LED);

    // BLE initialization
    simple_ble_app = simple_ble_init(&ble_config);

    // ^ init ----------- fun program times v

    // Start Advertising
    simple_ble_adv_only_name();

    // Generate a public key for the sensor
    error_code = mbedtls_ecdh_gen_public(
        &ctx_sensor.grp,            // Elliptic curve group
        &ctx_sensor.d,              // Sensor secret key
        &ctx_sensor.Q,              // Sensor public key
        mbedtls_ctr_drbg_random,    
        &ctr_drbg
    );
    APP_ERROR_CHECK(error_code);

    // Generate a key for the mule (XXX shouldn't stay here long-term)
    error_code = mbedtls_ecdh_gen_public(
        &ctx_mule.grp,            // Elliptic curve group
        &ctx_mule.d,              // Sensor secret key
        &ctx_mule.Q,              // Sensor public key
        mbedtls_ctr_drbg_random,    
        &ctr_drbg
    );
    APP_ERROR_CHECK(error_code);

    // Enter main loop.
    while (1) {
        nrf_gpio_pin_toggle(LED);
        nrf_delay_ms(1000);
        printf("beep!\n");
    }
}


