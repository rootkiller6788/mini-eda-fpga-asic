CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -I include
LDFLAGS = -lm

SRCDIR = src
EXDIR = examples
OUTDIR = bin

SRCS = $(SRCDIR)/fpga_arch.c $(SRCDIR)/lut_synth.c $(SRCDIR)/clb_pack.c \
       $(SRCDIR)/routing_fabric.c $(SRCDIR)/bitstream.c
OBJS = $(SRCS:$(SRCDIR)/%.c=$(SRCDIR)/%.o)

.PHONY: all test clean

all: $(OUTDIR) $(OUTDIR)/fpga_demo.exe

$(OUTDIR):
	-mkdir $(OUTDIR)

$(OUTDIR)/fpga_demo.exe: $(OBJS) $(EXDIR)/fpga_demo.c
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXDIR)/fpga_demo.c $(LDFLAGS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: all
	$(OUTDIR)/fpga_demo.exe

clean:
	-del /Q $(SRCDIR)\*.o 2>/dev/null
	-rmdir /S /Q $(OUTDIR) 2>/dev/null
