# main.py

from fastapi import FastAPI, Request, HTTPException
import inspect
import os
import asyncio
from concurrent.futures import ThreadPoolExecutor
from typing import Any, Dict

import appserver # Assuming app.py is in the same directory
import provider  # Assuming provider.py is in the same directory


app = FastAPI()
executor = ThreadPoolExecutor(max_workers=4*os.cpu_count())
# executor = ThreadPoolExecutor(max_workers=10)

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

@app.get("/public_params")
async def public_params(request: Request):
    return await make_threaded_call(request, provider.get_public_params, should_be_provider=True)

@app.post("/sign_tokens")
async def sign_tokens(request: Request):
    return await make_threaded_call(request, provider.sign_tokens, should_be_provider=True)

@app.post("/redeem_tokens")
async def redeem_tokens_endpoint(request: Request):
    payload = None
    try:
        payload = await request.json()
    except Exception as e:
        print("Error parsing body: {}".format(e))
        return {"result": 1}
    provider.process_redeem_tokens(payload)
    return {"result": 0}

@app.post("/deliver")
async def deliver(request: Request):
    return await make_threaded_call(request, appserver.deliver, should_be_provider=False)

