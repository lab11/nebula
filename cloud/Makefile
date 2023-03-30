TOKENLIB_WHL := anonymous_tokens_lib-0.1.0-cp310-cp310-macosx_10_7_x86_64.whl

.PHONY: provider app build clean

provider: build
	docker run -it --rm -p 8000:8000 -e SERVER_MODE=provider galaxy_cloud

app: build
	docker run -it --rm -p 8000:8000 -e SERVER_MODE=app galaxy_cloud

build:
	cd anonymous-tokens-lib && maturin build --release --out . && cd ..
	docker build -t galaxy_cloud --build-arg TOKENLIB_WHL=$(TOKENLIB_WHL) .

clean:
	rm -f anonymous-tokens-lib/$(TOKENLIB_WHL)
	rm -rf anonymous-tokens-lib/target
