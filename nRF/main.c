// BLE Service Template
//
// Creates a service for changing LED state over BLE

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "app_error.h"
#include "nrf.h"
#include "app_util.h"
#include "nrf_twi_mngr.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_serial.h"
#include "nrfx_gpiote.h"
#include "nrfx_saadc.h"

#include "simple_ble.h"
#include "buckler.h"

#include "max44009.h"

// Intervals for advertising and connections
static simple_ble_config_t ble_config = {
        // c0:98:e5:49:xx:xx
        .platform_id       = 0x49,    // used as 4th octect in device BLE address
        .device_id         = 0x1234, // TODO: replace with your lab bench number
        .adv_name          = "Galaxy_nRF52", // used in advertisements if there is room
        .adv_interval      = MSEC_TO_UNITS(1000, UNIT_0_625_MS),
        .min_conn_interval = MSEC_TO_UNITS(500, UNIT_1_25_MS),
        .max_conn_interval = MSEC_TO_UNITS(1000, UNIT_1_25_MS),
};

// 32e61089-2b22-4db5-a914-43ce41986c70
static simple_ble_service_t soil_service = {{
    .uuid128 = {0x70,0x6C,0x98,0x41,0xCE,0x43,0x14,0xA9,
                0xB5,0x4D,0x22,0x2B,0x89,0x10,0xE6,0x32}
}};

static simple_ble_char_t soil_char = {.uuid16 = 0x108a};
static bool led_state = true;

// ADC Channel 
#define SENSOR_CHANNEL 0

/*******************************************************************************
 *   State for this application
 ******************************************************************************/
// Main application state
simple_ble_app_t* simple_ble_app;


// callback for SAADC events
void saadc_callback (nrfx_saadc_evt_t const * p_event) {
  // don't care about adc callbacks
}

// sample a particular analog channel in blocking mode
nrf_saadc_value_t sample_value (uint8_t channel) {
  nrf_saadc_value_t val;
  ret_code_t error_code = nrfx_saadc_sample_convert(channel, &val);
  APP_ERROR_CHECK(error_code);
  return val;
}

int main(void) {

  // Initialize

  ret_code_t error_code = NRF_SUCCESS;

  // initialize RTT library
  error_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(error_code);
  NRF_LOG_DEFAULT_BACKENDS_INIT();

  // initialize analog to digital converter
  nrfx_saadc_config_t saadc_config = NRFX_SAADC_DEFAULT_CONFIG;
  saadc_config.resolution = NRF_SAADC_RESOLUTION_12BIT;
  error_code = nrfx_saadc_init(&saadc_config, saadc_callback);
  APP_ERROR_CHECK(error_code);

  // initialize analog inputs
  // configure with 0 as input pin for now
  nrf_saadc_channel_config_t channel_config = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(0);
  channel_config.gain = NRF_SAADC_GAIN1_6; // input gain of 1/6 Volts/Volt, multiply incoming signal by (1/6)
  channel_config.reference = NRF_SAADC_REFERENCE_INTERNAL; // 0.6 Volt reference, input after gain can be 0 to 0.6 Volts

  // specify input pin and initialize that ADC channel
  channel_config.pin_p = BUCKLER_GROVE_A1;
  error_code = nrfx_saadc_channel_init(SENSOR_CHANNEL, &channel_config);
  APP_ERROR_CHECK(error_code);

  // Setup Sensor GPIO
  //nrf_gpio_cfg_input(BUCKLER_GROVE_A1,NRF_GPIO_PIN_NOPULL);

  // initialization complete
  printf("Sensor ADC channel initialized!\n");

  // get sensor value
  nrf_saadc_value_t sensor_val = sample_value(SENSOR_CHANNEL);

  printf(sensor_val);


  // Setup BLE
  simple_ble_app = simple_ble_init(&ble_config);

  simple_ble_add_service(&soil_service);

  simple_ble_add_characteristic(1, 1, 0, 0,
      sizeof(sensor_val), (uint8_t*)&sensor_val,
      &soil_service, &soil_char);

  // Start Advertising
  simple_ble_adv_only_name();

  while(1) {
    power_manage();
  }
}

