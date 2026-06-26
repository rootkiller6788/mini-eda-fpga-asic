CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic -O2
INC     = -Iinclude
SRCDIR  = src
EXADIR  = examples
DEMDIR  = demos
TSTDIR  = tests
BENDIR  = benches
BLDDIR  = build

SRCS = $(SRCDIR)/noc_topology.c $(SRCDIR)/noc_router.c $(SRCDIR)/noc_routing.c \
       $(SRCDIR)/noc_flowctrl.c $(SRCDIR)/noc_perf.c \
       $(SRCDIR)/noc_qos.c $(SRCDIR)/noc_deadlock.c
OBJS = $(SRCS:$(SRCDIR)/%.c=$(BLDDIR)/%.o)

EXAS = $(BLDDIR)/ex01_mesh_routing $(BLDDIR)/ex02_deadlock_free \
       $(BLDDIR)/ex03_perf_model $(BLDDIR)/ex04_qos_demo
DEMS = $(BLDDIR)/demo01_noc_sim $(BLDDIR)/demo02_topology_compare
TEST = $(BLDDIR)/test_all
BENCH = $(BLDDIR)/bench_throughput

.PHONY: all clean test examples demos bench run-test

all: examples demos test

$(BLDDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(BLDDIR)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

examples: $(EXAS)
demos:    $(DEMS)
test:     $(TEST)
bench:    $(BENCH)

$(BLDDIR)/ex01_mesh_routing: $(EXADIR)/ex01_mesh_routing.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

$(BLDDIR)/ex02_deadlock_free: $(EXADIR)/ex02_deadlock_free.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

$(BLDDIR)/ex03_perf_model: $(EXADIR)/ex03_perf_model.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

$(BLDDIR)/ex04_qos_demo: $(EXADIR)/ex04_qos_demo.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

$(BLDDIR)/demo01_noc_sim: $(DEMDIR)/demo01_noc_sim.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

$(BLDDIR)/demo02_topology_compare: $(DEMDIR)/demo02_topology_compare.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

$(BLDDIR)/test_all: $(TSTDIR)/test_all.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

$(BLDDIR)/bench_throughput: $(BENDIR)/bench_throughput.c $(OBJS)
	$(CC) $(CFLAGS) $(INC) -o $@ $^

run-test: $(TEST)
	./$(TEST)
	@echo "=== TEST SUITE PASSED ==="

clean:
	rm -rf $(BLDDIR)
