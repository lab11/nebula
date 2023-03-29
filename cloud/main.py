# main.py

from fastapi import FastAPI, Request
from typing import Dict, Any
import threading
import provider  # Assuming provider.py is in the same directory
import app  # Assuming app.py is in the same directory

app = FastAPI()

class FunctionThread(threading.Thread):
    def __init__(self, target, args=(), kwargs=None):
        super().__init__()
        self.target = target
        self.args = args
        self.kwargs = kwargs if kwargs is not None else {}
        self.result = None

    def run(self):
        self.result = self.target(*self.args, **self.kwargs)

@app.get("/run_provider")
async def run_provider(request: Request):
    params = dict(request.query_params)

    def call_provider_threaded():
        return provider.provider(**params)

    thread = FunctionThread(target=call_provider_threaded)
    thread.start()
    thread.join()

    return {"result": thread.result}

@app.get("/run_app")
async def run_app(request: Request):
    params = dict(request.query_params)

    def call_app_threaded():
        return app.app(**params)

    thread = FunctionThread(target=call_app_threaded)
    thread.start()
    thread.join()

    return {"result": thread.result}

