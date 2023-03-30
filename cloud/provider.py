# provider.py
import tokenlib

def provider(a: int) -> int:

    keypair = tokenlib.generate_keypair()                          # PROVIDER
    pubparams = tokenlib.get_public_params(keypair)                # PROVIDER

    blinded_token = tokenlib.generate_token(pubparams)             # APP SERVER
    signed_token = tokenlib.sign_token(keypair, blinded_token)     # PROVIDER
    token = tokenlib.unblind_token(blinded_token, signed_token)    # APP SERVER
    #token = bytes(bytearray(len(token)))

    is_valid_token = tokenlib.verify_token(keypair, token)         # PROVIDER
    print("token is valid?", is_valid_token)

    return 2*a

provider(1)