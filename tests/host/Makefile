# Host unit tests for the ProtoView decoders (no Flipper hardware needed).
# Usage: make -C tests/host test

ROOT := ../..
CC ?= cc
SAN := -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all
# -Wno-format: host uint32_t is 'unsigned int', the firmware's is 'unsigned
# long', so existing %lu logging would not compile here. ufbt checks formats.
CFLAGS := -std=gnu11 -g -O1 -Wall -Wextra -Werror -Wno-unused-parameter \
          -Wno-missing-field-initializers -Wno-format $(SAN) \
          -Istubs -I$(ROOT) -I. -DAPP_ROOT=\"$(ROOT)\"
LDFLAGS := $(SAN) -lm

APP_SRCS := $(ROOT)/signal.c $(ROOT)/fields.c $(ROOT)/crc.c $(ROOT)/raw_samples.c \
            $(wildcard $(ROOT)/protocols/*.c) $(wildcard $(ROOT)/protocols/tpms/*.c)
TEST_SRCS := test_main.c test_vw.c test_pipeline.c

BIN := build/host_tests

.PHONY: test clean $(BIN)  # always rebuild: make's 1s mtime resolution can miss edits

test: $(BIN)
	./$(BIN)

$(BIN): $(APP_SRCS) $(TEST_SRCS) test_util.h $(wildcard stubs/*.h stubs/*/*.h) Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) $(APP_SRCS) $(TEST_SRCS) -o $@ $(LDFLAGS)

clean:
	rm -rf build
