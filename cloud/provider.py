# provider.py
import tokenlib
import util
import platform_db
import platform_tokendb
import queue
import threading
from concurrent.futures import ThreadPoolExecutor


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


def consumer(batch_queue):
    batch_size = 14000  # Define a batch size for DB writes
    batch = []

    while True:
        token = batch_queue.get()

        if token is None:
            # Put the sentinel back for other potential consumers
            batch_queue.put(None)
            break

        batch.append(token)

        if len(batch) >= batch_size:
            num_unused = token_db.add_new_elements(batch)
            #mule_db.batch_increment_counts([('mule', num_unused)])
            batch = []

    # Write any remaining elements in the batch
    if batch:
        num_unused = token_db.add_new_elements(batch)

def redeem_tokens(payload) -> int:
    # Define a queue and start the consumer
    batch_queue = queue.Queue()
    consumer_thread = threading.Thread(target=consumer, args=(batch_queue,), daemon=True)
    consumer_thread.start()

    decoded_tokens = [util.decode_bytes(token) for token in payload["tokens"]]
    valid_tokens = [token for token in decoded_tokens if tokenlib.verify_token(_keypair, token)]

    # Put valid tokens into the queue
    for token in valid_tokens:
        batch_queue.put(token)

    # Signal to the consumer that the producer has finished
    batch_queue.put(None)

    return 0
