
// Creates a service sending data over the Galaxy network

#include <stdbool.h>
#include <stdint.h>
#include "nrf.h"
#include "app_util.h"
#include "nrf_twi_mngr.h"
#include "nrf_gpio.h"
#include "display.h"

#include "galaxy_ble.h"
//#include "simple_ble.h"
#include "buckler.h"

#include "max44009.h"

// Intervals for advertising and connections
static galaxy_ble_config_t ble_config = {
        // c0:98:e5:49:xx:xx
        .platform_id       = 0x49,    // used as 4th octect in device BLE address
        .device_id         = 0x1234, // TODO: replace with your lab bench number
        .adv_name          = "Galaxy_nRF52", // used in advertisements if there is room
        .adv_interval      = MSEC_TO_UNITS(1000, UNIT_0_625_MS),
        .min_conn_interval = MSEC_TO_UNITS(500, UNIT_1_25_MS),
        .max_conn_interval = MSEC_TO_UNITS(1000, UNIT_1_25_MS),
};

// 32e61089-2b22-4db5-a914-43ce41986c70
static galaxy_ble_service_t galaxy_service = {{
    .uuid128 = {0x70,0x6C,0x98,0x41,0xCE,0x43,0x14,0xA9,
                0xB5,0x4D,0x22,0x2B,0x89,0x10,0xE6,0x32}
}};

static galaxy_ble_char_t galaxy_state_char = {.uuid16 = 0x108a};
static bool galaxy_state = true;

/*******************************************************************************
 *   State for this application
 ******************************************************************************/
// Main application state
galaxy_ble_app_t* galaxy_ble_app;

void ble_evt_write(ble_evt_t const* p_ble_evt) {  //TODO need to rewrite for certification steps 
    if (galaxy_ble_is_char_event(p_ble_evt, &galaxy_state_char)) {
      printf("Got write to LED characteristic!\n");
      if (galaxy_state) {
        printf("Turning on LED!\n");
        nrf_gpio_pin_clear(BUCKLER_LED0);
      } else {
        printf("Turning off LED!\n");
        nrf_gpio_pin_set(BUCKLER_LED0);
      }
    }
}

int main(void) {

  // Setup BLE
  galaxy_ble_app = galaxy_ble_init(&ble_config);

  galaxy_ble_add_service(&galaxy_service);

  galaxy_ble_add_characteristic(1, 1, 0, 0,
      sizeof(galaxy_state), (uint8_t*)&galaxy_state,
      &galaxy_service, &galaxy_state_char);

  //TODO: send certificate 
  float cert = 83743627.034729;
  galaxy_ble_adv_certificate((uint8_t*) &cert, sizeof(cert));
  printf("sent certificate\n");


  //exchange and check certificates 


  //TODO: connect and send data 

  // Start Advertising
  //galaxy_ble_adv_only_name();

  while(1) {
    power_manage();
  }
}

