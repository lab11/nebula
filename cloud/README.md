# Galaxy Cloud - Provider and Application Servers

## Build and launch the Docker
```bash
docker build -t my_app .
docker run -it --rm -p 8000:8000 -e SERVER_MODE=provider my_app
```

## HTTPS Get request
```bash
http://0.0.0.0:8000/run_provider?a=1
```

## Provider API

* `GET public_params` --> input: nothing, returns: public parameter payload
* `GET sign_tokens` --> input: list of blinded tokens, returns: list of signed blinded tokens
* `GET redeem_tokens` --> input: list of unblinded tokens, returns: count of valid tokens

## App Server API

* `GET deliver` --> input: data payload, returns: unblinded token

## Steps for Rust->Python bindings [JL notes]

1. run maturin develop
2. run python script