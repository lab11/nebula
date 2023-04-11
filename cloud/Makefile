.PHONY: provider app build docker_setup docker_kill_servers local clean keypair

PORT=8000
DEFAULT_PROVIDER_URL=http://provider

provider: build docker_setup 
	docker run -it --rm -p 8000:$(PORT)\
	 -e SERVER_PORT=$(PORT) -e SERVER_MODE=provider galaxy_cloud

app: build docker_setup
	docker run -it --rm -p 8080:$(PORT)\
	 -e SERVER_PORT=$(PORT) -e SERVER_MODE=app -e PROVIDER_URL=$(DEFAULT_PROVIDER_URL) galaxy_cloud

local: build docker_setup
	docker run -t --rm -p 8000:$(PORT) --network galaxy_net --name provider --hostname provider\
	 -e SERVER_PORT=$(PORT) -e SERVER_MODE=provider galaxy_cloud &
	docker run -it --rm -p 8080:$(PORT) --network galaxy_net --name appserver --hostname appserver\
	 -e SERVER_PORT=$(PORT) -e SERVER_MODE=app -e PROVIDER_URL=$(DEFAULT_PROVIDER_URL):$(PORT) galaxy_cloud

build:
	mkdir -p _build
	cp -r ../ext/anonymous-tokens _build/
	docker build -t galaxy_cloud .

docker_kill_servers:
	docker stop provider || true
	docker stop appserver || true
	docker rm provider || true
	docker rm appserver || true

docker_setup: docker_kill_servers
	docker network create galaxy_net || true

clean: docker_kill_servers
	rm -rf _build
	docker network rm galaxy_net

keypair:
	maturin develop --manifest-path anonymous-tokens-lib/Cargo.toml --release
	python gen_keypair.py
