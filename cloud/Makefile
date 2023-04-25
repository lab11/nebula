.PHONY: build local remote clean destroy-local destroy-remote keypair

local: build
	cd tf/local && terraform init && terraform apply -auto-approve

docker: build
	docker tag galaxy_cloud gcr.io/opportunistic-networks-galaxy/galaxy_cloud:latest
	docker push gcr.io/opportunistic-networks-galaxy/galaxy_cloud:latest

remote: build
	docker tag galaxy_cloud gcr.io/opportunistic-networks-galaxy/galaxy_cloud:latest
	docker push gcr.io/opportunistic-networks-galaxy/galaxy_cloud:latest

	cd tf/gcp && terraform init && terraform apply -auto-approve

build:
	mkdir -p _build
	cp -r ../ext/anonymous-tokens _build/
	docker build -t galaxy_cloud .

clean: 
	rm -rf _build

destroy-local:
	cd tf/local && terraform destroy -auto-approve

destroy-remote:
	cd tf/gcp && terraform destroy -auto-approve

keypair:
	maturin develop --manifest-path anonymous-tokens-lib/Cargo.toml --release
	python gen_keypair.py
