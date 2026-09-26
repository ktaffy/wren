PYTHON ?= python3

.PHONY: build test

build:
	$(MAKE) -C c

test: build
	PYTHON=$(PYTHON) ./scripts/test.sh