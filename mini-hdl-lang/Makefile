CC=gcc
CFLAGS=-Wall -Wextra -O2 -I include
OUTDIR=bin

SRCS=src/verilog_sim.c src/vhdl_sim.c src/systemverilog_sim.c src/testbench_gen.c src/waveform_view.c

.PHONY: all clean

all: $(OUTDIR) $(OUTDIR)/verilog_counter $(OUTDIR)/vhdl_adder $(OUTDIR)/sv_fifo

$(OUTDIR):
	if not exist "$(OUTDIR)" mkdir "$(OUTDIR)"

$(OUTDIR)/verilog_counter: examples/verilog_counter.c $(SRCS)
	$(CC) $(CFLAGS) -o $@ examples/verilog_counter.c $(SRCS)

$(OUTDIR)/vhdl_adder: examples/vhdl_adder.c $(SRCS)
	$(CC) $(CFLAGS) -o $@ examples/vhdl_adder.c $(SRCS)

$(OUTDIR)/sv_fifo: examples/sv_fifo.c $(SRCS)
	$(CC) $(CFLAGS) -o $@ examples/sv_fifo.c $(SRCS)

clean:
	if exist "$(OUTDIR)" rmdir /s /q "$(OUTDIR)"
