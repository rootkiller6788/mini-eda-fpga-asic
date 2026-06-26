# Makefile — mini-hardware-verification
CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2 -I.
LDFLAGS  = -lm

# Headers
HDRS = uvm_methodology.h assertion_check.h formal_verify.h coverage_mdl.h verification_ip.h

# Sources
SRCS = uvm_methodology.c assertion_check.c formal_verify.c coverage_mdl.c verification_ip.c
OBJS = $(SRCS:.c=.o)

# Examples
EXAMPLES = example_uvm_tb example_assertion example_coverage

# Demos
DEMOS = demo_verif_flow demo_regression

# All targets
ALL_TARGETS = $(EXAMPLES) $(DEMOS)

.PHONY: all clean test assertions coverage demo regression

all: $(ALL_TARGETS)

# Object files from sources
%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Example: UVM Testbench
example_uvm_tb: example_uvm_tb.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) $(LDFLAGS) -o $@

# Example: Assertions
example_assertion: example_assertion.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) $(LDFLAGS) -o $@

# Example: Coverage
example_coverage: example_coverage.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) $(LDFLAGS) -o $@

# Demo: Full Verification Flow
demo_verif_flow: demo_verif_flow.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) $(LDFLAGS) -o $@

# Demo: Regression Testing
demo_regression: demo_regression.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) $(LDFLAGS) -o $@

# Run tests
test: example_uvm_tb
	./example_uvm_tb

assertions: example_assertion
	./example_assertion

coverage: example_coverage
	./example_coverage

demo: demo_verif_flow
	./demo_verif_flow

regression: demo_regression
	./demo_regression

# Run all
check: test assertions coverage

# Clean
clean:
	del /Q *.o 2>nul
	del /Q $(ALL_TARGETS:=.exe) 2>nul
	del /Q $(ALL_TARGETS) 2>nul
