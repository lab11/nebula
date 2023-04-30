#include "aes_gcm.h"
#include "nrf_crypto.h"
#include "nrf_crypto_aes.h"

void encrypt_character_array(const uint8_t *key, const uint8_t *iv, const uint8_t *plaintext, uint8_t *payload, size_t length)
{
    ret_code_t ret_val;
    nrf_crypto_aes_context_t aes_ctx;

    // Initialize AES-128 GCM context
    ret_val = nrf_crypto_aes_init(&aes_ctx, &g_nrf_crypto_aes_gcm_256_info, NRF_CRYPTO_ENCRYPT);
    if (ret_val != NRF_SUCCESS)
    {
        printf("Failed to initialize AES context. Error: 0x%x\n", ret_val);
        return;
    }

    // Set the encryption key
    ret_val = nrf_crypto_aes_key_set(&aes_ctx, key);
    if (ret_val != NRF_SUCCESS)
    {
        printf("Failed to set AES key. Error: 0x%x\n", ret_val);
        return;
    }

    // Set the IV
    //ret_val = nrf_crypto_aes_iv_set(&aes_ctx, iv, NRF_CRYPTO_AES_IV_SIZE);
    if (ret_val != NRF_SUCCESS)
    {
        printf("Failed to set AES IV. Error: 0x%x\n", ret_val);
        return;
    }

    // Encrypt the plaintext using AES-256 GCM
    uint8_t ciphertext[length];
    ret_val = nrf_crypto_aes_update(&aes_ctx, plaintext, length, ciphertext);
    if (ret_val != NRF_SUCCESS)
    {
        printf("Encryption failed. Error: 0x%x\n", ret_val);
        return;
    }

    // Compute the authentication tag
    uint8_t tag[NRF_CRYPTO_AES_IV_SIZE];
    //ret_val = nrf_crypto_aes_finalize(&aes_ctx, tag, NRF_CRYPTO_AES_IV_SIZE);
    if (ret_val != NRF_SUCCESS)
    {
        printf("Failed to compute authentication tag. Error: 0x%x\n", ret_val);
        return;
    }

    // Create the payload with the structure: IV || Ciphertext || Authentication Tag
    memcpy(payload, iv, NRF_CRYPTO_AES_IV_SIZE);
    memcpy(payload + NRF_CRYPTO_AES_IV_SIZE, ciphertext, length);
    memcpy(payload + NRF_CRYPTO_AES_IV_SIZE + length, tag, NRF_CRYPTO_AES_IV_SIZE);
}

