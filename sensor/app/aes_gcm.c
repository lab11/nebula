#include "aes_gcm.h"
#include "nrf_crypto.h"
#include "nrf_crypto_aes.h"

void encrypt_character_array(const uint8_t *key, const uint8_t *nounce, const uint8_t *plaintext, uint8_t *payload, size_t length)
{
    ret_code_t ret_val;
    nrf_crypto_aead_context_t aes_ctx;
    

    printf("init aes\n");

    // Initialize AES-256 GCM context
    ret_val = nrf_crypto_aead_init(&aes_ctx, &g_nrf_crypto_aes_gcm_256_info, key);
    if (ret_val != NRF_SUCCESS)
    {
        printf("Failed to initialize AES context. Error: 0x%x\n", ret_val);
        return;
    }

    printf("make tag\n");

    // Compute the authentication tag
    uint8_t tag[NRF_CRYPTO_AES_TAG_SIZE]; 
    //ret_val = nrf_crypto_aes_finalize(&aes_ctx, tag, NRF_CRYPTO_AES_IV_SIZE);
    if (ret_val != NRF_SUCCESS)
    {
        printf("Failed to compute authentication tag. Error: 0x%x\n", ret_val);
        return;
    }

    printf("encrypt\n");

    // Encrypt the plaintext using AES-256 GCM
    uint8_t ciphertext[length];
    ret_val = nrf_crypto_aead_crypt(&aes_ctx, NRF_CRYPTO_ENCRYPT, nounce, NRF_CRYPTO_AES_NOUCE_SIZE, NULL,
                                 0, plaintext, length, ciphertext, tag, NRF_CRYPTO_AES_TAG_SIZE);
    if (ret_val != NRF_SUCCESS)
    {
        printf("Encryption failed. Error: 0x%x\n", ret_val);
        return;
    }


    printf("nounce: ");
    for (int i = 0; i < 12; i++)
    {
        printf("%02x", nounce[i]);
    }
    printf("\n");

    printf("tag: ");
    for (int i = 0; i < 16; i++)
    {
        printf("%02x", tag[i]);
    }
    printf("\n");

    printf("ciphertext: ");
    for (int i = 0; i < length; i++)
    {
        printf("%02x", ciphertext[i]);
    }
    printf("\n");

    // Create the payload with the structure: IV || Ciphertext || Authentication Tag
    memcpy(payload, nounce, NRF_CRYPTO_AES_NOUCE_SIZE);
    memcpy(payload + NRF_CRYPTO_AES_NOUCE_SIZE, ciphertext, length);
    memcpy(payload + NRF_CRYPTO_AES_NOUCE_SIZE + length, tag, NRF_CRYPTO_AES_TAG_SIZE);

}

