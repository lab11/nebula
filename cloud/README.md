# Galaxy Cloud - Provider and Application Servers

## Databases on the Provider
* provider_db.py runs a database with schema {mule_id, count}
* platform_tokendb.py maintains a set of tokens.  

We now use REDIS for platform_tokendb.py. 

Install REDIS

```bash
curl -fsSL https://packages.redis.io/gpg | sudo gpg --dearmor -o /usr/share/keyrings/redis-archive-keyring.gpg

echo "deb [signed-by=/usr/share/keyrings/redis-archive-keyring.gpg] https://packages.redis.io/deb $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/redis.list

sudo apt-get update
sudo apt-get install redis
```

## AES
* pip install cryptography
* payload structure is as follows: IV (12 bytes) || Ciphertext || Authentication Tag (16 bytes).


## Build instructions

Use the included `Makefile` to run the provider and application servers in various configurations.

 * `make build`: compiles a Docker image that runs the provider or application server
 * `make clean`: cleans build files

### Generate provider keypair

 * `make keypair`: generates a `keypair.bin` file that the provider Docker image will package as credentials to sign and verify tokens. Don't commit this file to Github :)

### Local

Running locally uses Terraform to spin up some Docker containers. See `tf/local/main.tf` for the up-to-date configuration. Should expose the provider at `localhost:8000` and the application server at `localhost:8080`

 * `make local`: compiles and runs both provider application server images using Terraform.
 * `make destroy-local`: removes Terraform resources and shuts down images.

### GCP

The provider and application servers run as GCP Cloud Run services at the following URLs:

* Provider: `https://provider-vavytk2tca-uc.a.run.app`
* Application Server: `https://appserver-1-vavytk2tca-uc.a.run.app`

To deploy, you'll first need to install and configure the GCP SDK (e.g. `brew install google-cloud-sdk`) and authenticate:

1. `gcloud init`
2. `gcloud auth application-default login`
3. `gcloud auth configure-docker`

Then you'll be able to deploy the services (note: the API will be publicly accessible):

 * `make remote`: Tags the galaxy docker image and pushes it to the Google Container Registry, then starts the services using Terraform.
 * `make destroy-local`: shuts down Terraform-instantiated services.

## API

### Provider

    * `GET /public_params`
        Params: (none)
        Body:   (none)

        Returns:
            {
                "result": <encoded public parameter str>
            }

    * `GET /complaint_public_params`
        Params: (none)
        Body:   (none)

        Returns:
            {
                "result": <encoded complaint public parameter str (diff. from above)>
            }

    * `POST /sign_tokens`
        Params: (none)
        Body: 
            Content-type: application/json
            {
                "mode": <either "data" or "complaint">,
                "blinded_tokens": [<encoded blinded token strs>]
            }

        Returns:
            {
                "result": [<encoded signed token strs>]
            }
            
        // If the mode is "data", then the provider uses the normal keypair to sign all the tokens. If the mode is "complaint", then the provider uses the complaint keypair to sign UP TO <X> tokens (let's say, 100). Assume we check identity, which we don't rn.

    * `POST /redeem_tokens`
        Params: (none)
        Body:
            Content-type: application/json
            {
                "tokens": [<encoded token strs>]
            }

        Returns:
            {
                "result": <number valid tokens>
            }

    * `POST /query_provider`
        Params: (none)
        Body:
            Content-type: application/json
            {
                "tokens": [<encoded token strs>]
            }

        Returns:
            {
                "result": {
                    "invalid_tokens": [...],
                    "duplicate_tokens": [...]
                }
            }
            
    * `POST /complain`
        Params: (none)
        Body:
            Content-type: application/json
            {
                "token": <encoded token str>,
                "blinded_token": <encoded token str>,
                "complaint": <encoded complaint bytes, not important>,
                "payload": <bytes, might be empty>,
                "sig": <bytes, might be empty>
            }

        Returns:
            {
                "result": <signed token or empty>
            }
            
        // Verifies that the token was signed correctly using the complaint key and calls some function that processes the data bytes

### Application Server

    * `POST /predeliver`
        Params: (none)
        Body:
            Content-type: application/json
            {
                "data": <predeliver data>
            }

        Returns:
            {
                "result": <encoded response bytes>
            }

    * `POST /deliver`
        Params: (none)
        Body:
            Content-type: application/json
            {
                "data": <payload data>
            }

        Returns:
            {
                "result": <encoded token payload>
            }
