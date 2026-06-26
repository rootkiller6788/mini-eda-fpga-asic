CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -I include
LDFLAGS = -lm

SRCDIR = src
TESTDIR = tests
EXDIR = examples
OUTDIR = bin

SRCS = $(SRCDIR)/fpga_arch.c $(SRCDIR)/bitstream_gen.c $(SRCDIR)/clb_pack.c \
       $(SRCDIR)/routing_fabric.c $(SRCDIR)/timing_fpga.c $(SRCDIR)/techmap_fpga.c \
       $(SRCDIR)/place_fpga.c $(SRCDIR)/io_planning.c $(SRCDIR)/fpga_flow.c
OBJS = $(SRCS:$(SRCDIR)/%.c=$(SRCDIR)/%.o)

TESTS = test_fpga_arch test_bitstream test_clb_pack test_routing \
        test_timing test_techmap test_place test_io test_flow

.PHONY: all test clean

all: $(OUTDIR) $(addprefix $(OUTDIR)/, $(TESTS:=.exe))

$(OUTDIR):
	-mkdir $(OUTDIR)

$(OUTDIR)/test_fpga_arch.exe: $(OBJS) $(TESTDIR)/test_fpga_arch.c
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(TESTDIR)/test_fpga_arch.c $(LDFLAGS)

$(OUTDIR)/test_bitstream.exe: $(OBJS) $(TESTDIR)/test_bitstream.c
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(TESTDIR)/test_bitstream.c $(LDFLAGS)

$(OUTDIR)/test_clb_pack.exe: $(OBJS) $(TESTDIR)/test_clb_pack.c
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(TESTDIR)/test_clb_pack.c $(LDFLAGS)

$(OUTDIR)/test_routing.exe: $(OBJS) $(TESTDIR)/test_routing.c
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(TESTDIR)/test_routing.c $(LDFLAGS)

$(OUTDIR)/test_timing.exe: $(OBJS) $(TESTDIR)/test_timing.c
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(TESTDIR)/test_timing.c $(LDFLAGS)

$(OUTDIR)/test_techmap.exe: $(OBJS) $(TESTDIR)/test_techmap.c
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(TESTDIR)/test_techmap.c $(LDFLAGS)

$(OUTDIR)/test_place.exe: $(OBJS) $(TESTDIR)/test_place.c
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(TESTDIR)/test_place.c $(LDFLAGS)

$(OUTDIR)/test_io.exe: $(OBJS) $(TESTDIR)/test_io.c
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(TESTDIR)/test_io.c $(LDFLAGS)

$(OUTDIR)/test_flow.exe: $(OBJS) $(TESTDIR)/test_flow.c
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(TESTDIR)/test_flow.c $(LDFLAGS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Individual test targets
test_fa: $(OUTDIR)/test_fpga_arch.exe
	$(OUTDIR)/test_fpga_arch.exe

test_bs: $(OUTDIR)/test_bitstream.exe
	$(OUTDIR)/test_bitstream.exe

test_cp: $(OUTDIR)/test_clb_pack.exe
	$(OUTDIR)/test_clb_pack.exe

test_rt: $(OUTDIR)/test_routing.exe
	$(OUTDIR)/test_routing.exe

test_tm: $(OUTDIR)/test_timing.exe
	$(OUTDIR)/test_timing.exe

test_tmap: $(OUTDIR)/test_techmap.exe
	$(OUTDIR)/test_techmap.exe

test_pl: $(OUTDIR)/test_place.exe
	$(OUTDIR)/test_place.exe

test_io: $(OUTDIR)/test_io.exe
	$(OUTDIR)/test_io.exe

test_fl: $(OUTDIR)/test_flow.exe
	$(OUTDIR)/test_flow.exe

test: all
	@echo ===== FPGA Architecture Test =====
	$(OUTDIR)/test_fpga_arch.exe
	@echo ""
	@echo ===== Bitstream Test =====
	$(OUTDIR)/test_bitstream.exe
	@echo ""
	@echo ===== CLB Packing Test =====
	$(OUTDIR)/test_clb_pack.exe
	@echo ""
	@echo ===== Routing Test =====
	$(OUTDIR)/test_routing.exe
	@echo ""
	@echo ===== Timing Test =====
	$(OUTDIR)/test_timing.exe
	@echo ""
	@echo ===== TechMap Test =====
	$(OUTDIR)/test_techmap.exe
	@echo ""
	@echo ===== Placement Test =====
	$(OUTDIR)/test_place.exe
	@echo ""
	@echo ===== I/O Planning Test =====
	$(OUTDIR)/test_io.exe
	@echo ""
	@echo ===== Full Flow Test =====
	$(OUTDIR)/test_flow.exe

testall: test

clean:
	-del /Q $(SRCDIR)\*.o 2>nul
	-rmdir /S /Q $(OUTDIR) 2>nul
