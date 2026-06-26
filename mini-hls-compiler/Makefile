CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -pedantic -O2 -Iinclude
AR       = ar
ARFLAGS  = rcs

SRC      = $(wildcard src/*.c)
OBJ      = $(SRC:.c=.o)
LIB      = libhls.a

EXAMPLES = examples/example_hls

.PHONY: all clean examples test

all: $(LIB) examples

$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

examples/%: examples/%.c $(LIB)
	$(CC) $(CFLAGS) -o $@.exe $< -L. -lhls

examples: $(EXAMPLES)

test: all
	./examples/example_hls.exe

clean:
	del /q src\*.o $(LIB) examples\*.exe 2>/dev/null || rm -f src/*.o $(LIB) examples/*.exe
