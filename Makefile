IMAGE ?= lightbench-build:lab2

.PHONY: image build clean

image:
	docker build -t $(IMAGE) docker

build: image
	docker run --rm -v "$(CURDIR):/workspace" -w /workspace \
		--user "$(shell id -u):$(shell id -g)" $(IMAGE) \
		make -C firmware all BUILD_ID=$(shell git rev-parse --short HEAD)-$(shell cat firmware/App/*.c firmware/Platform/runtime.c | shasum -a 256 | cut -c1-8)

clean:
	$(MAKE) -C firmware clean

.PHONY: check test sanitize coverage test-linux
check:
	python3 tools/check_project.py
test: check
	$(MAKE) -C tests test
	python3 tests/test_export.py
sanitize: check
	$(MAKE) -C tests sanitize
test-linux: image
	docker run --rm -v "$(CURDIR):/workspace" -w /workspace --user "$(shell id -u):$(shell id -g)" $(IMAGE) make test sanitize
coverage: image
	docker run --rm -v "$(CURDIR):/workspace" -w /workspace --user "$(shell id -u):$(shell id -g)" $(IMAGE) python3 tools/coverage.py
