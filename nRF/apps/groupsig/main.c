// Groupsig app
//
// Blinks an LED and does a group signature :0

// #include <stdbool.h>
// #include <stdint.h>

#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"

// #include "groupsig.h"

// Pin definitions
#define LED NRF_GPIO_PIN_MAP(0,17)

int main(void) {

    // Initialize.
    nrf_gpio_cfg_output(LED);

    // Try to run groupsig hello world
    groupsig_hello_world();

    // Enter main loop.
    while (1) {
        nrf_gpio_pin_toggle(LED);
        nrf_delay_ms(500);
    }
}

