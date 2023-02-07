// Blink app
//
// Blinks an LED

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

/*******************************************************************************
 *   State for this application
 ******************************************************************************/
// Main application state
simple_ble_app_t* simple_ble_app;

int main(void) {

  ret_code_t error_code = NRF_SUCCESS;
  error_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(error_code);
  NRF_LOG_DEFAULT_BACKENDS_INIT();

  //error_code = nrf_crypto_init();

  printf("Log initialized!\n");

  // Initialize.
  nrf_gpio_cfg_output(LED);

   // Setup BLE
  simple_ble_app = simple_ble_init(&ble_config);

  // Start Advertising
  simple_ble_adv_only_name();

  // Enter main loop.
  while (1) {
    nrf_gpio_pin_toggle(LED);
    nrf_delay_ms(1000);
    printf("hello?\n");
    }
        
}


