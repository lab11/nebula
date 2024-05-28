# Galaxy Cloud - Provider and Application Servers

## Build instructions

Use the included `Makefile` to run the provider and application servers in various configurations.

 * `make build`: compiles a Docker image that runs the provider or application server
 * `make clean`: cleans build files

To run:

 1. Initialize and update all submodules in the top-level repository folder: `git submodule update --init --recursive`. This may take a while.

 2. Install python packages: `pip install maturin pycryptodome requests `

 3. Install [Docker](https://docs.docker.com/engine/install) for your environment. I got things to work by giving my user root access to the docker daemon [as described here](https://docs.docker.com/engine/install/linux-postinstall/), but be careful that this gives your user a lot of privileges.

 4. Install Rust: `curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh` and switch to the nightly build: `rustup default nightly`

 5. We use a REDIS database for the Provider, so you'll want to install REDIS next:

 ```bash
 curl -fsSL https://packages.redis.io/gpg | sudo gpg --dearmor -o /usr/share/keyrings/redis-archive-keyring.gpg

 echo "deb [signed-by=/usr/share/keyrings/redis-archive-keyring.gpg] https://packages.redis.io/deb $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/redis.list

 sudo apt-get update
 sudo apt-get install redis
 ```

 6. Install [Terraform](https://developer.hashicorp.com/terraform/install) for your environment.

 7. Run `make keypair` to generate a `keypair.bin` file that the provider Docker image will package as credentials to sign and verify tokens. Don't commit this file to Github :)

### Local

Running locally uses Terraform to spin up some Docker containers. See `tf/local/main.tf` for the up-to-date configuration. Should expose the provider at `localhost:8000` and the application server at `localhost:8080`. You many need to run these commands as sudo to use the Docker service.

 * `make local`: compiles and runs both provider application server images using Terraform.
 * `make destroy-local`: removes Terraform resources and shuts down images.

## Test Script

For an example of how to interact with a provider-appserver pair running in local containers, look at the `test_mule.py` script.

------

### GCP

*NOTE*: GCP instructions are old and likely kinda broken -- try at your own risk :)

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
