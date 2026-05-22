CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -I include
LDFLAGS = -lm

SRCDIR = src
EXDIR = examples
OUTDIR = bin

SRCS = $(SRCDIR)/logic_synth.c $(SRCDIR)/tech_map.c $(SRCDIR)/place_route.c \
       $(SRCDIR)/static_timing.c $(SRCDIR)/eda_flow.c
OBJS = $(SRCS:$(SRCDIR)/%.c=$(SRCDIR)/%.o)

DEMOS = synth_demo techmap_demo placeroute_demo sta_demo flow_demo

.PHONY: all test clean

all: $(OUTDIR) $(addprefix $(OUTDIR)/, $(DEMOS:=.exe))

$(OUTDIR):
	-mkdir $(OUTDIR)

$(OUTDIR)/synth_demo.exe: $(OBJS) $(EXDIR)/synth_demo.c
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXDIR)/synth_demo.c $(LDFLAGS)

$(OUTDIR)/techmap_demo.exe: $(OBJS) $(EXDIR)/techmap_demo.c
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXDIR)/techmap_demo.c $(LDFLAGS)

$(OUTDIR)/placeroute_demo.exe: $(OBJS) $(EXDIR)/placeroute_demo.c
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXDIR)/placeroute_demo.c $(LDFLAGS)

$(OUTDIR)/sta_demo.exe: $(OBJS) $(EXDIR)/sta_demo.c
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXDIR)/sta_demo.c $(LDFLAGS)

$(OUTDIR)/flow_demo.exe: $(OBJS) $(EXDIR)/flow_demo.c
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(EXDIR)/flow_demo.c $(LDFLAGS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

synth_demo: $(OUTDIR)/synth_demo.exe
	$(OUTDIR)/synth_demo.exe

techmap_demo: $(OUTDIR)/techmap_demo.exe
	$(OUTDIR)/techmap_demo.exe

placeroute_demo: $(OUTDIR)/placeroute_demo.exe
	$(OUTDIR)/placeroute_demo.exe

sta_demo: $(OUTDIR)/sta_demo.exe
	$(OUTDIR)/sta_demo.exe

flow_demo: $(OUTDIR)/flow_demo.exe
	$(OUTDIR)/flow_demo.exe

test: all
	@echo ===== Synth Demo =====
	$(OUTDIR)/synth_demo.exe
	@echo.
	@echo ===== Techmap Demo =====
	$(OUTDIR)/techmap_demo.exe
	@echo.
	@echo ===== Place & Route Demo =====
	$(OUTDIR)/placeroute_demo.exe
	@echo.
	@echo ===== STA Demo =====
	$(OUTDIR)/sta_demo.exe
	@echo.
	@echo ===== Full Flow Demo =====
	$(OUTDIR)/flow_demo.exe

clean:
	-del /Q $(SRCDIR)\*.o 2>nul
	-rmdir /S /Q $(OUTDIR) 2>nul
