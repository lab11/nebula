.PHONY: provider app build clean

provider: build
	docker run -it --rm -p 8000:8000 -e SERVER_MODE=provider galaxy_cloud

app: build
	docker run -it --rm -p 8000:8000 -e SERVER_MODE=app galaxy_cloud

build:
	mkdir -p _build
	cp -r ../ext/anonymous-tokens _build/
	docker build -t galaxy_cloud --progress=plain .

clean:
	rm -rf _build
