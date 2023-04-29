import base64
import tokenlib

# util function -- convert bytes to base64 string
def to_base64(data: bytes) -> str:
    return base64.b64encode(data).decode('utf-8')


# util function -- convert base64 string to bytes
def from_base64(data: str) -> bytes:
    return base64.b64decode(data)


'''
Generates a signed, unblinded token given the input keypair. This is
insecure -- it combines provider and app server functionality -- but it is
useful for testing.

Input: keypair (bytes) -- read from `keypair.bin` or similar

Output: token (string) -- a valid, signed token encoded as a base64 string
'''
def get_token(keypair):

    public_params = tokenlib.get_public_params(keypair)

    blinded_token = tokenlib.generate_token(public_params)
    return to_base64(
        tokenlib.unblind_token(blinded_token, tokenlib.sign_token(keypair, blinded_token))
    )

'''
Example usage -- assumes `keypair.bin` exists in the parent directory (e.g. ../keypair.bin)
'''
if __name__ == "__main__":

    # load keypair (generated with gen_keypair.py)
    with open('../keypair.bin', 'rb') as f:
        keypair = f.read()

    t = get_token(keypair)

    print()
    print("token:", t)
    print("  verifies?", "yes :)" if tokenlib.verify_token(keypair, from_base64(t)) else "no >:(")
    print()
