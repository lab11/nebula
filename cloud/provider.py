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
    #print("DEBUG: returning public params {}".format( _keypair))
    return util.encode_bytes(tokenlib.get_public_params(_keypair))


def sign_tokens(payload) -> list[str]:

    decoded_tokens = [util.decode_bytes(token) for token in payload["blinded_tokens"]]
    signed_tokens = [tokenlib.sign_token(_keypair, token) for token in decoded_tokens]

    return [util.encode_bytes(token) for token in signed_tokens]


def redeem_tokens(payload) -> int:

    #print("DEBUG: Redeeming tokens")

    decoded_tokens = [util.decode_bytes(token) for token in payload["tokens"]]
    #print("DEBUG: decoded request tokens ({}): {}".format(len(decoded_tokens), decoded_tokens))

    invalid_tokens = []
    for t in decoded_tokens:
        #print("  calling verify_token with keypair {} and token {}".format(_keypair, t))
        is_valid = tokenlib.verify_token(_keypair, t)
        #print("  result:", "valid" if is_valid else "not valid")
        if not is_valid:
            invalid_tokens.append(t)

    #print("DEBUG: invalid tokens ({}): {}".format(len(invalid_tokens), invalid_tokens)) 

    valid_tokens = [token for token in decoded_tokens if token not in invalid_tokens]
    #print("DEBUG: valid tokens ({}): {}".format(len(valid_tokens), valid_tokens))

    num_unused = token_db.add_new_elements(valid_tokens)
    #print("DEBUG: added valid tokens to token database and {} were unused".format(num_unused))

    #print("DEBUG: incrementing number of valid tokens for mule id `{}` by {}".format('mule', num_unused))
    mule_db.batch_increment_counts([('mule', num_unused)])

    #print("DEBUG: returning {}".format(num_unused))
    return num_unused

