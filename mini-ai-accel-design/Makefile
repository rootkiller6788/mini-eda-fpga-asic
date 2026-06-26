CC=gcc ; CFLAGS=-std=c99 -Wall -Wextra -pedantic -O2 -Iinclude ; AR=ar ; ARFLAGS=rcs
SRC_FILES=tensor_core.c dataflow_dse.c sparse_engine.c memory_hierarchy.c accel_roofline.c
OBJ=$(SRC_FILES:%.c=src/%.o) ; LIB=libaiaccel.a
.PHONY: all clean test
all: $(LIB) ; $(CC) $(CFLAGS) examples/example_ai_accel.c -L. -laiaccel -lm -o examples/example_ai_accel.exe
$(LIB): $(OBJ) ; $(AR) $(ARFLAGS) $@ $^
src/%.o: src/%.c ; $(CC) $(CFLAGS) -c -o $@ $<
test: all ; ./examples/example_ai_accel.exe
clean: ; rm -f src/*.o $(LIB) examples/*.exe
