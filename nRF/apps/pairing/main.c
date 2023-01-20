// RTT test app
//
// Says hello

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_serial.h"

#include "bls12_381.h"

uint32_t rnd_state;
uint32_t xorshift(uint32_t *state) {
    uint32_t x = *state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    
    *state = x;
    return x;
}

void get_xorshift_bytes(void *buf, size_t n) {

    size_t remaining = n;
    while (remaining > sizeof(uint32_t)) {
        printf("remaining: %d\n", remaining);
        uint32_t rnd = xorshift(&rnd_state); 

        buf = memcpy(buf, &rnd, sizeof(uint32_t));
        remaining -= sizeof(uint32_t);
    } 

    uint32_t rnd = xorshift(&rnd_state); 
    buf = memcpy(buf, &rnd, remaining); 
}

int main(void) {

    ret_code_t error_code = NRF_SUCCESS;
    error_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(error_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    printf("Log initialized!\n");

    embedded_pairing_bls12_381_g1_t a;
    printf("got to %d\n", __LINE__);
    embedded_pairing_bls12_381_g1_random(&a, get_xorshift_bytes);
    printf("got to %d\n", __LINE__);

    embedded_pairing_bls12_381_g1_t b;
    embedded_pairing_bls12_381_g1_random(&b, get_xorshift_bytes);

    bool does_this_work = embedded_pairing_bls12_381_g1_equal(&a, &b);

    // Enter main loop.
    while (1) {
        nrf_delay_ms(1000);
        printf("hello?\n");
        printf("did it work? %d\n", does_this_work);
    }
}

