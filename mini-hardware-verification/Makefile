# mini-hardware-verification Makefile
# Hardware Functional Verification Framework
#
# Usage:
#   make        - build all libraries and tests
#   make test   - build and run tests
#   make examples - build example programs
#   make clean  - remove build artifacts
#   make all    - build everything (lib, tests, examples)

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -g
INC      = -I./include
LDFLAGS  = -lm

SRCDIR   = src
INCDIR   = include
TESTDIR  = tests
EXDIR    = examples
BUILDDIR = build

# Source files
SRCS     = $(wildcard $(SRCDIR)/*.c)
OBJS     = $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SRCS))

# Headers
HEADERS  = $(wildcard $(INCDIR)/*.h)

# Test
TEST_SRC = $(TESTDIR)/test_all.c
TEST_BIN = $(BUILDDIR)/test_all

# Examples
EXAMPLES = $(BUILDDIR)/example_fifo_verify \
           $(BUILDDIR)/example_riscv_verify \
           $(BUILDDIR)/example_bus_verify

# Static library
LIB      = $(BUILDDIR)/libhwverify.a

.PHONY: all test examples clean dirs

all: dirs $(LIB) $(TEST_BIN) $(EXAMPLES)

dirs:
	@mkdir -p $(BUILDDIR)

# Library
$(LIB): $(OBJS)
	ar rcs $@ $^

# Object files
$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

# Test binary
$(TEST_BIN): $(TEST_SRC) $(LIB) $(HEADERS) | dirs
	$(CC) $(CFLAGS) $(INC) $< -L$(BUILDDIR) -lhwverify $(LDFLAGS) -o $@

# Example: FIFO verification
$(EXAMPLES): | dirs
$(BUILDDIR)/example_fifo_verify: $(EXDIR)/example_fifo_verify.c $(LIB) $(HEADERS)
	$(CC) $(CFLAGS) $(INC) $< -L$(BUILDDIR) -lhwverify $(LDFLAGS) -o $@

# Example: RISC-V ALU verification
$(BUILDDIR)/example_riscv_verify: $(EXDIR)/example_riscv_verify.c $(LIB) $(HEADERS)
	$(CC) $(CFLAGS) $(INC) $< -L$(BUILDDIR) -lhwverify $(LDFLAGS) -o $@

# Example: AXI Bus verification
$(BUILDDIR)/example_bus_verify: $(EXDIR)/example_bus_verify.c $(LIB) $(HEADERS)
	$(CC) $(CFLAGS) $(INC) $< -L$(BUILDDIR) -lhwverify $(LDFLAGS) -o $@

# Run tests (make test - one-click pass)
test: $(TEST_BIN)
	@echo "========================================"
	@echo "  mini-hardware-verification Test Suite"
	@echo "========================================"
	@echo ""
	@LD_LIBRARY_PATH=$(BUILDDIR):$$LD_LIBRARY_PATH $(TEST_BIN)
	@echo ""
	@echo "Test suite complete."

# Build examples
examples: $(EXAMPLES)

# Run all examples
run-examples: $(EXAMPLES)
	@echo "Running FIFO verification example..."
	@LD_LIBRARY_PATH=$(BUILDDIR):$$LD_LIBRARY_PATH $(BUILDDIR)/example_fifo_verify
	@echo ""
	@echo "Running RISC-V ALU verification example..."
	@LD_LIBRARY_PATH=$(BUILDDIR):$$LD_LIBRARY_PATH $(BUILDDIR)/example_riscv_verify
	@echo ""
	@echo "Running AXI Bus verification example..."
	@LD_LIBRARY_PATH=$(BUILDDIR):$$LD_LIBRARY_PATH $(BUILDDIR)/example_bus_verify

# Clean
clean:
	rm -rf $(BUILDDIR)

# Line count
count:
	@echo "=== Line Counts ==="
	@echo "Headers:"
	@wc -l $(INCDIR)/*.h
	@echo ""
	@echo "Sources:"
	@wc -l $(SRCDIR)/*.c
	@echo ""
	@echo "Tests:"
	@wc -l $(TESTDIR)/*.c
	@echo ""
	@echo "Examples:"
	@wc -l $(EXDIR)/*.c
	@echo ""
	@echo "Total include/ + src/:"
	@cat $(INCDIR)/*.h $(SRCDIR)/*.c | wc -l

# Help
help:
	@echo "mini-hardware-verification Build System"
	@echo "  make          - build everything"
	@echo "  make test     - build and run tests"
	@echo "  make examples - build examples"
	@echo "  make clean    - remove build artifacts"
	@echo "  make count    - line count statistics"
