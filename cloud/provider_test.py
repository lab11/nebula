import sqlite3
from fastapi import FastAPI, Request, HTTPException
from concurrent.futures import ThreadPoolExecutor
import tokenlib
import os
import asyncio
import base64

app = FastAPI()
executor = ThreadPoolExecutor(max_workers=os.cpu_count())
# load keypair (generated with gen_keypair.py)
with open('keypair.bin', 'rb') as f:
    _keypair = f.read()

def decode_bytes(data: str) -> bytes:
    return base64.b64decode(data)


class StringSet:
    def __init__(self):
        self.conn = sqlite3.connect("strings.db", check_same_thread=False)
        self._create_table()

    def _create_table(self):
        cursor = self.conn.cursor()
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS strings (
                value TEXT PRIMARY KEY
            )
        """)
        self.conn.commit()

    def add_new_elements(self, new_strings):
        cursor = self.conn.cursor()
        cursor.executemany("INSERT OR IGNORE INTO strings (value) VALUES (?)", [(string,) for string in new_strings])
        self.conn.commit()
        return cursor.rowcount

    def close(self):
        self.conn.close()

def redeem_tokens(payload) -> int:
    decoded_tokens = [decode_bytes(token) for token in payload["tokens"]]
    valid_tokens = [token for token in decoded_tokens if tokenlib.verify_token(_keypair, token)]
    num_unused = token_db.add_new_elements(valid_tokens)
    mule_db.batch_increment_counts([('mule', num_unused)])
    return num_unused

async def make_threaded_call(request: Request, fn):
    params = dict(request.query_params)
    body_json = None
    try: 
        body_json = await request.json()
    except Exception as e:
        print("Error parsing body: {}".format(e))


    def call_function_threaded():
        return fn(**params, payload=body_json)

    loop = asyncio.get_running_loop()
    result = await loop.run_in_executor(executor, call_function_threaded)
    return {"result": result}

@app.post("/redeem_tokens")
async def redeem_tokens_endpoint(request: Request):
    return await make_threaded_call(request, redeem_tokens)
