from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.backends import default_backend

def decrypt_data(payload, key):
    nounce_size = 12
    tag_size = 16

    nounce = payload[:nounce_size]
    tag = payload[-tag_size:]
    ciphertext = payload[nounce_size:-tag_size]

    print("Nounce:", nounce)
    print("Tag:", tag)
    print("Ciphertext:", ciphertext)

    cipher = Cipher(algorithms.AES(key), modes.GCM(nounce, tag), backend=default_backend())
    decryptor = cipher.decryptor()
    plaintext = decryptor.update(ciphertext) + decryptor.finalize()

    return plaintext

if __name__=="__main__":
    # Assuming payload is a bytes-like object containing the received data
    payload = bytes.fromhex("000000000000000000000000fb953d1b67d8617f75530594032f6361fddd3f30e80aaf4c20b49c0a9d20edec5b2e")
    key = bytes([0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F])

    print("Payload:",payload)
    plaintext = decrypt_data(payload, key)
    print("Decrypted data:", plaintext.decode('utf-8'))
    #print("Decrypted data:", plaintext.hex())

