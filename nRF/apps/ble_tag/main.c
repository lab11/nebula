// BLE advertisement for airtab/tile detection experiment
//
// Advertises device name: Lab11 Tag

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "nrf.h"
#include "app_util.h"
#include "nrf_twi_mngr.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "buckler.h"
#include "simple_ble.h"

// Create a timer
APP_TIMER_DEF(adv_timer);

// Create a counter 
int counter;
counter = 0;

// Intervals for advertising and connections
simple_ble_config_t ble_config = {
        // c0:98:e5:49:xx:xx
        .platform_id       = 0x49,    // used as 4th octect in device BLE address
        .device_id         = 0x0000,  // 
        .adv_name          = "Lab11 Tag", // Note that this name is not displayed to save room in the advertisement for data.
        .adv_interval      = MSEC_TO_UNITS(1000, UNIT_0_625_MS),
        .min_conn_interval = MSEC_TO_UNITS(500, UNIT_1_25_MS),
        .max_conn_interval = MSEC_TO_UNITS(1000, UNIT_1_25_MS),
};

// Main application state
simple_ble_app_t* simple_ble_app;

void rotate_mac() {
  uint32_t err_code;
  printf("rotate mac function called\n");

  advertising_stop();

  //ble_config.device_id = 0x0001;
  if (counter == 2) {

    ble_gap_addr_t gap_addr = {.addr_type = BLE_GAP_ADDR_TYPE_PUBLIC};
    sd_ble_gap_addr_get(&gap_addr);

    // Iterate MAC address
    uint8_t new_ble_addr[6] = {gap_addr.addr[0]+1, 0x00, 0x49, 0xe5, 0x98, 0xc0};
    memcpy(gap_addr.addr, new_ble_addr, 6);
    err_code = sd_ble_gap_addr_set(&gap_addr);
    APP_ERROR_CHECK(err_code);

    counter = 0;
    printf("rotated\n");

  }
  else {
    counter += 1;
    printf("iterated\n");
  }

  // Advertise MAC and name 
  simple_ble_adv_only_name();

}


int main(void) {
  ret_code_t error_code = NRF_SUCCESS;

  // Initialize RTT library
  error_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(error_code);
  NRF_LOG_DEFAULT_BACKENDS_INIT();
  printf("Log initialized\n");

  // Setup BLE
  simple_ble_app = simple_ble_init(&ble_config);

  // Advertise MAC and name 
  simple_ble_adv_only_name();

  printf("done with that\n");

  // Set a timer to rotate the MAC and update advertisement data every second.
  app_timer_init();
  app_timer_create(&adv_timer, APP_TIMER_MODE_REPEATED, (app_timer_timeout_handler_t) rotate_mac);
  app_timer_start(adv_timer, APP_TIMER_TICKS(300000), NULL); // 5 min 

  while(1) {
    // Sleep while SoftDevice handles BLE
    power_manage();

    //if advertising stops restart it
  }
}

