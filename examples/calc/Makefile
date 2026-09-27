PYTHON ?= python3
CC     ?= cc

REPO     := $(abspath ../..)
BUILD    := build
GEN      := $(BUILD)/gen
LIB_SRCS := $(wildcard $(REPO)/c/src/*.c) $(wildcard $(REPO)/c/src/server/*.c)
WRENGEN  := $(wildcard $(REPO)/wrengen/src/wrengen/*.py)
CFLAGS   := -Wall -g -D_GNU_SOURCE -I$(REPO)/c/include -I$(GEN)

.PHONY: all clean
all: $(BUILD)/calc_server $(GEN)/calc.py

$(GEN)/%.h $(GEN)/%.c: %.wren $(WRENGEN)
	$(PYTHON) -m wrengen $< --lang c --out $(GEN)

$(GEN)/%.py: %.wren $(WRENGEN)
	$(PYTHON) -m wrengen $< --lang py --out $(GEN)

$(BUILD)/calc_server: server.c $(GEN)/calc.c $(GEN)/calc.h $(LIB_SRCS)
	$(CC) $(CFLAGS) -o $@ server.c $(GEN)/calc.c $(LIB_SRCS) -lm

clean:
	rm -rf $(BUILD)