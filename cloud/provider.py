# provider.py
import tokenlib
import util

# load keypair (generated with gen_keypair.py)
with open('keypair.bin', 'rb') as f:
    _keypair = f.read()


def get_public_params(payload=None) -> str:
    return util.encode_bytes(tokenlib.get_public_params(_keypair))


def sign_tokens(payload) -> list[str]:

    decoded_tokens = [util.decode_bytes(token) for token in payload["blinded_tokens"]]
    signed_tokens = [tokenlib.sign_token(_keypair, token) for token in decoded_tokens]

    return [util.encode_bytes(token) for token in signed_tokens]


def redeem_tokens(payload) -> int:

    decoded_tokens = [util.decode_bytes(token) for token in payload["tokens"]]
    return sum([tokenlib.verify_token(_keypair, token) for token in decoded_tokens])
