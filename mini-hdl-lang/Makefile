CC=gcc
CFLAGS=-Wall -Wextra -O2 -I include
OUTDIR=bin

SRCS=src/verilog_sim.c src/vhdl_sim.c src/systemverilog_sim.c src/testbench_gen.c src/waveform_view.c
NEWSRCS=src/hdl_lexer.c src/hdl_parser.c src/hdl_ast.c src/hdl_elaborate.c src/hdl_codegen.c
ALLSRCS=$(SRCS) $(NEWSRCS)

.PHONY: all clean test

all: $(OUTDIR)/verilog_counter $(OUTDIR)/vhdl_adder $(OUTDIR)/sv_fifo

test: $(OUTDIR)/test_hdl
	$(OUTDIR)/test_hdl

$(OUTDIR)/verilog_counter: examples/verilog_counter.c $(SRCS) | $(OUTDIR)
	$(CC) $(CFLAGS) -o $@ examples/verilog_counter.c $(SRCS)

$(OUTDIR)/vhdl_adder: examples/vhdl_adder.c $(SRCS) | $(OUTDIR)
	$(CC) $(CFLAGS) -o $@ examples/vhdl_adder.c $(SRCS)

$(OUTDIR)/sv_fifo: examples/sv_fifo.c $(SRCS) | $(OUTDIR)
	$(CC) $(CFLAGS) -o $@ examples/sv_fifo.c $(SRCS)

$(OUTDIR)/test_hdl: tests/test_hdl.c $(ALLSRCS) | $(OUTDIR)
	$(CC) $(CFLAGS) -o $@ tests/test_hdl.c $(ALLSRCS)

$(OUTDIR):
	mkdir -p $(OUTDIR)

clean:
	rm -rf $(OUTDIR)
