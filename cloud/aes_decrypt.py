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
    payload = b'your_payload_here'
    key = bytes([0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F])
    plaintext = decrypt_data(payload, key)
    print("Decrypted data:", plaintext.decode('utf-8'))

