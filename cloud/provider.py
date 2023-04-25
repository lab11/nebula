# provider.py
import tokenlib
import util
import platform_db
import platform_tokendb

# load keypair (generated with gen_keypair.py)
with open('keypair.bin', 'rb') as f:
    _keypair = f.read()

mule_db = platform_db.KeyValueDatabase()
token_db = platform_tokendb.StringSet()


def get_public_params(payload=None) -> str:
    return util.encode_bytes(tokenlib.get_public_params(_keypair))


def sign_tokens(payload) -> list[str]:

    decoded_tokens = [util.decode_bytes(token) for token in payload["blinded_tokens"]]
    signed_tokens = [tokenlib.sign_token(_keypair, token) for token in decoded_tokens]

    return [util.encode_bytes(token) for token in signed_tokens]


def redeem_tokens(payload) -> int:

    decoded_tokens = [util.decode_bytes(token) for token in payload["tokens"]]

    invalid_tokens = [token for token in decoded_tokens if tokenlib.verify_token(_keypair, token)]
    valid_tokens = [token for token in decoded_tokens if token not in invalid_tokens]

    num_unused = token_db.add_new_elements(valid_tokens)

    mule_db.batch_increment_counts([('mule', num_unused)])
    return num_unused

