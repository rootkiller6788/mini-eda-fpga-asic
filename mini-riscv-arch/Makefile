CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -pedantic -O2 -Iinclude
AR       = ar
ARFLAGS  = rcs

SRC      = riscv_csr.c riscv_priv.c riscv_except.c riscv_mmu.c riscv_debug.c
OBJ      = $(SRC:%.c=src/%.o)
LIB      = libriscv.a

.PHONY: all clean test

all: $(LIB) examples/example_riscv.exe

$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

examples/example_riscv.exe: examples/example_riscv.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -L. -lriscv

test: all
	./examples/example_riscv.exe

clean:
	del /q src\*.o $(LIB) examples\*.exe 2>/dev/null || rm -f src/*.o $(LIB) examples/*.exe
