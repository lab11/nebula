
#include "backhaul.h"

#include <string.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"

/*
#define TOKEN_BYTES 88
#define MAX_TOKENS 1000
*/
const char *redeem_prefix = "{\"tokens\":[";
const char *redeem_suffix = "]}";

// We're going to construct the payload inline in this buffer as the tokens roll in
char token_storage[
    sizeof(redeem_prefix) +
    (TOKEN_BYTES + 3) * MAX_TOKENS + // each token gets quotes around it and a comma
    (-1) + // the last comma gets replaced with a closing bracket
    sizeof(redeem_suffix)
];

int n_tokens;

// -- WiFi shenanigans --

static esp_err_t wifi_evt_handler(void *ctx, system_event_t *event) {
    return ESP_OK;
}


// -----

char *token_from_idx(int idx) {
    return token_storage + sizeof(redeem_prefix) + (idx * (TOKEN_BYTES + 3));
}

void backhaul_reset() {
    n_tokens = 0;
    memcpy(token_storage, redeem_prefix, sizeof(redeem_prefix));
}

int backhaul_upload_data(char *payload, int payload_len) {
    if (n_tokens >= MAX_TOKENS) {
        printf("Ran out of space to store tokens (n_tokens < %d)\n", MAX_TOKENS);
        return 0;
    }

    char *token = token_from_idx(n_tokens);

    // send the payload to the server and save the token at token

    n_tokens++;
}

int backhaul_num_tokens() {
    return n_tokens;
}

void finalize_payload() {
    // replace the last comma with a closing bracket
    char *suffix_ptr = token_from_idx(n_tokens) - 1;
    memcpy(suffix_ptr, redeem_suffix, sizeof(redeem_suffix));
}

int backhaul_redeem_tokens() {

    finalize_payload();

    // redeem the tokens at the server
    int num_tokens_redeemed = 0;

    n_tokens = 0;

    return num_tokens_redeemed;
}