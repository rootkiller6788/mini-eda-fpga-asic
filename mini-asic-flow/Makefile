CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic -O2
INC     = -Iinclude
SRCDIR  = src
EXADIR  = examples
DEMDIR  = demos
BLDDIR  = build

SRCS = $(SRCDIR)/rtl_to_gds.c $(SRCDIR)/std_cells.c $(SRCDIR)/floorplan_cts.c \
       $(SRCDIR)/eco_flow.c $(SRCDIR)/drc_lvs.c
OBJS = $(SRCS:$(SRCDIR)/%.c=$(BLDDIR)/%.o)

EXAS = $(BLDDIR)/ex01_asic_flow $(BLDDIR)/ex02_std_cells $(BLDDIR)/ex03_floorplan_cts
DEMS = $(BLDDIR)/demo01_signoff    $(BLDDIR)/demo02_drc_lvs

.PHONY: all clean examples demos

all: examples demos

$(BLDDIR)/%.o: $(SRCDIR)/%.c
	@if not exist "$(BLDDIR)" mkdir "$(BLDDIR)"
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

examples: $(EXAS)
demos:    $(DEMS)

$(BLDDIR)/ex01_asic_flow: $(EXADIR)/ex01_asic_flow.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

$(BLDDIR)/ex02_std_cells: $(EXADIR)/ex02_std_cells.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

$(BLDDIR)/ex03_floorplan_cts: $(EXADIR)/ex03_floorplan_cts.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

$(BLDDIR)/demo01_signoff: $(DEMDIR)/demo01_signoff.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

$(BLDDIR)/demo02_drc_lvs: $(DEMDIR)/demo02_drc_lvs.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

clean:
	@if exist "$(BLDDIR)" rmdir /s /q "$(BLDDIR)"
