#ifndef AES_GCM_H
#define AES_GCM_H

#include <stdint.h>
#include <stddef.h>

#define NRF_CRYPTO_AES_KEY_SIZE 32 // AES-256 bit key size
#define NRF_CRYPTO_AES_IV_SIZE 12 // AES GCM uses a 12-byte IV

void encrypt_character_array(const uint8_t *key, const uint8_t *iv, const uint8_t *plaintext, uint8_t *payload, size_t length);

#endif // AES_GCM_H

