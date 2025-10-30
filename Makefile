CC ?= gcc
CFLAGS ?= -std=c99 -pedantic -Wvla -Wall -Wshadow -O3
SRCS = pa2.c
BIN = pa2
EXAMPLES_DIR = examples

.PHONY: all test test-3 test-8 clean zip

all: $(BIN)

$(BIN): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(BIN)

test: test-3 test-8

test-3: $(BIN)
	@echo "=== Running test 3 ==="
	@mkdir -p test_out
	@./$(BIN) $(EXAMPLES_DIR)/3.txt test_out/3_1.dim test_out/3_1.pck test_out/3_all.dim test_out/3_all.pck
	@echo "--- diffs for 3 ---"
	@diff -u $(EXAMPLES_DIR)/3_1.dim test_out/3_1.dim || true
	@diff -u $(EXAMPLES_DIR)/3_1.pck test_out/3_1.pck || true
	@diff -u $(EXAMPLES_DIR)/3_all.dim test_out/3_all.dim || true
	@diff -u $(EXAMPLES_DIR)/3_all.pck test_out/3_all.pck || true

test-8: $(BIN)
	@echo "=== Running test 8 ==="
	@mkdir -p test_out
	@./$(BIN) $(EXAMPLES_DIR)/8.txt test_out/8_1.dim test_out/8_1.pck test_out/8_all.dim test_out/8_all.pck
	@echo "--- diffs for 8 ---"
	@diff -u $(EXAMPLES_DIR)/8_1.dim test_out/8_1.dim || true
	@diff -u $(EXAMPLES_DIR)/8_1.pck test_out/8_1.pck || true
	@diff -u $(EXAMPLES_DIR)/8_all.dim test_out/8_all.dim || true
	@diff -u $(EXAMPLES_DIR)/8_all.pck test_out/8_all.pck || true

clean:
	@rm -f $(BIN)
	@rm -rf test_out

zip: $(BIN)
	@zip -j pa2.zip $(SRCS) Makefile
