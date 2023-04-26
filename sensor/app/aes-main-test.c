#include <stdio.h>
#include "aes_gcm.h"

/*
int main(void)
{
    uint8_t key[NRF_CRYPTO_AES_KEY_SIZE] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    uint8_t iv[NRF_CRYPTO_AES_IV_SIZE] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
                                          0xA8, 0xA9, 0xAA, 0xAB};

    uint8_t plaintext[] = "Your message here!";
    size_t length = sizeof(plaintext) - 1; // Subtract 1 to ignore the null-terminator
    size_t payload_length = NRF_CRYPTO_AES_IV_SIZE + length + NRF_CRYPTO_AES_IV_SIZE;
    uint8_t payload[payload_length];

    // Encrypt the character array
    //encrypt_character_array(key, iv, plaintext, payload, length);

    // Print the encrypted payload
    printf("Encrypted payload: ");
    for (size_t i = 0; i < payload_length; i++)
    {
        printf("%02x ", payload[i]);
    }
    printf("\n");

    return 0;
}
*/
