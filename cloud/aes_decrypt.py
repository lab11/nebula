from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.backends import default_backend

def decrypt_data(payload, key):
    iv_size = 12
    tag_size = 16

    iv = payload[:iv_size]
    tag = payload[-tag_size:]
    ciphertext = payload[iv_size:-tag_size]

    cipher = Cipher(algorithms.AES(key), modes.GCM(iv, tag), backend=default_backend())
    decryptor = cipher.decryptor()
    plaintext = decryptor.update(ciphertext) + decryptor.finalize()

    return plaintext

if __name__=="__main__":
    # Assuming payload is a bytes-like object containing the received data
    payload = bytes.fromhex("fb 01 00 00 34 03 01 20 d4 94 00 20 c8 94 00 20 01 07 00 00 9c b6 05 00 b0 10 01 20 7c fe 05 00 30 09 01 20 10 13 01 20 3c 00")
    key = bytes([0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F])

    print("Payload:",payload)
    plaintext = decrypt_data(payload, key)
    print("Decrypted data:", plaintext.decode('utf-8'))

