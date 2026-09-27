CC ?= cc
BUILD := build
INC := -I../c/include
C_SRCS := $(wildcard ../c/src/*.c) $(wildcard ../c/src/server/*.c)

.PHONY: all clean

all: $(BUILD)/loadgen $(BUILD)/server

$(BUILD)/loadgen: loadgen.c ../c/src/proto.c | $(BUILD)
	$(CC) -O2 -Wall -Wextra -D_GNU_SOURCE $(INC) -o $@ $^ -pthread

$(BUILD)/server: $(C_SRCS) ../c/examples/main.c | $(BUILD)
	$(CC) -O2 -Wall -D_GNU_SOURCE $(INC) -o $@ $^

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)