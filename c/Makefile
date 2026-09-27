CC       = gcc
CC_FLAGS = -Wall -g -D_GNU_SOURCE
INCLUDE  = -I include

SRC_DIR     = src
EXAMPLE_DIR = examples
BUILD_DIR   = build
TEST_DIR = tests
OBJECT_DIR  = $(BUILD_DIR)/obj
RELEASE_DIR = $(BUILD_DIR)/release
TARGET      = $(RELEASE_DIR)/server

LIB_SRCS = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/server/*.c)
LIB_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJECT_DIR)/%.o,$(LIB_SRCS))

EX_SRCS = $(wildcard $(EXAMPLE_DIR)/*.c)
EX_OBJS = $(patsubst $(EXAMPLE_DIR)/%.c,$(OBJECT_DIR)/examples/%.o,$(EX_SRCS))

TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_BINS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/tests/%,$(TEST_SRCS))

OBJS = $(LIB_OBJS) $(EX_OBJS)
PREFIX ?= /usr/local
LIB     = $(BUILD_DIR)/libwren.a

.PHONY: all run clean test lib install

all: $(TARGET)

run: $(TARGET)
	./$(TARGET)

lib: $(LIB)

$(LIB): $(LIB_OBJS)
	ar rcs $@ $^

install: $(LIB)
	install -d $(DESTDIR)$(PREFIX)/lib/pkgconfig $(DESTDIR)$(PREFIX)/include/wren
	install -m 644 $(LIB) $(DESTDIR)$(PREFIX)/lib/
	install -m 644 include/wren/*.h $(DESTDIR)$(PREFIX)/include/wren/
	printf 'prefix=%s\nincludedir=$${prefix}/include\nlibdir=$${prefix}/lib\n\nName: wren\nDescription: wren RPC server runtime\nVersion: 0.1.0\nCflags: -I$${includedir}\nLibs: -L$${libdir} -lwren\n' \
	    '$(PREFIX)' > $(DESTDIR)$(PREFIX)/lib/pkgconfig/wren.pc

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

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do $$t || exit 1; done

$(BUILD_DIR)/tests/%: $(TEST_DIR)/%.c $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CC_FLAGS) $(INCLUDE) -o $@ $^