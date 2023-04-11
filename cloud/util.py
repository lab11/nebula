# util.py
import base64

def encode_bytes(data: bytes) -> str:
    return base64.b64encode(data).decode('utf-8')


def decode_bytes(data: str) -> bytes:
    return base64.b64decode(data)
