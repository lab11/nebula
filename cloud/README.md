# Galaxy Cloud - Provider and Application Servers

## Build instructions

Use the included `Makefile` to run the provider and application servers in various configurations.

    * `make build`: compiles a Docker image that runs the provider or application server
    * `make clean`: cleans build files and local Docker containers/networks

### Generate provider keypair

    * `make keypair`: generates a `keypair.bin` file that the provider Docker image will package as credentials to sign and verify tokens. Don't commit this file to Github :)

### Local

    * `make provider`: compiles and runs the provider server Docker image at port 8000
    * `make app`: compiles and runs the app server Docker image at port 8080
    * `make local`: compiles and runs both a provider image and an application server image at ports 8000 and 8080, respectively. THe images are connected together on a `galaxy_cloud` bridge network.

## API

### Provider

    * `GET /public_params`
        Params: (none)
        Body:   (none)

        Returns:
            {
                "result": <encoded public parameter str>
            }

    * `GET /sign_tokens`
        Params: (none)
        Body: 
            Content-type: application/json
            {
                "blinded_tokens": [<encoded blinded token strs>]
            }

        Returns:
            {
                "result": [<encoded signed token strs>]
            }

    * `GET /redeem_tokens`
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

### Application Server

    * `GET /deliver`
        Params: (none)
        Body:
            Content-type: application/json
            {
                "data": <payload data>
            }

        Returns:
            {
                "result": <encoded token str>
            }
