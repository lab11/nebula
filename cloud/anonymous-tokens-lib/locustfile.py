import json
from locust import HttpUser, task, between, events
import resource
from token_gen import get_token
resource.setrlimit(resource.RLIMIT_NOFILE, (999999, 999999))

import json
from locust import HttpUser, task, between

class TokenRedemption(HttpUser):
    wait_time = between(1, 2)

    @task
    def redeem_tokens(self):
        headers = {
            'Content-Type': 'application/json',
        }

        data = {
            'tokens': ['ztuAgIfPmgZMpZcarGRwQXfuTpAPuRXIU5LUNdVQMr3Y9YlgwJIwueNYqU3cnibdXQzTHB/J/YcV26iB49i+Uw==' for x in range(300)]
            # 'tokens': [get_token(keypair) for x in range(300)]
        }

        self.client.post(
            '/redeem_tokens',
            headers=headers,
            data=json.dumps(data),
            verify=False,
        )