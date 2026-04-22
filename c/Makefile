CC       = gcc
CC_FLAGS = -Wall -g -D_GNU_SOURCE
INCLUDE  = -I include

SRC_DIR     = src
EXAMPLE_DIR = examples
BUILD_DIR   = build
OBJECT_DIR  = $(BUILD_DIR)/obj
RELEASE_DIR = $(BUILD_DIR)/release
TARGET      = $(RELEASE_DIR)/server

LIB_SRCS = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/server/*.c)
LIB_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJECT_DIR)/%.o,$(LIB_SRCS))

EX_SRCS = $(wildcard $(EXAMPLE_DIR)/*.c)
EX_OBJS = $(patsubst $(EXAMPLE_DIR)/%.c,$(OBJECT_DIR)/examples/%.o,$(EX_SRCS))

OBJS = $(LIB_OBJS) $(EX_OBJS)

.PHONY: all run clean

all: $(TARGET)

run: $(TARGET)
	./$(TARGET)

$(TARGET): $(OBJS) | $(RELEASE_DIR)
	$(CC) $(CC_FLAGS) -o $@ $^

$(OBJECT_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CC_FLAGS) $(INCLUDE) -c $< -o $@

$(OBJECT_DIR)/examples/%.o: $(EXAMPLE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CC_FLAGS) $(INCLUDE) -c $< -o $@

$(RELEASE_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)