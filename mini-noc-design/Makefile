CC       = clang
CFLAGS   = -std=c99 -Wall -Wextra -O2 -Wno-unused-parameter
LDFLAGS  =

SRCS     = noc_topology.c vc_wormhole.c axi_protocol.c noc_performance.c noc_verilog_rtl.c
OBJS     = $(SRCS:.c=.o)
HDRS     = noc_topology.h vc_wormhole.h axi_protocol.h noc_performance.h noc_verilog_rtl.h

EXAMPLES = example_mesh_topology example_vc_wormhole example_axi_noc_bridge
DEMOS    = demo_noc_simulation demo_router_pipeline

.PHONY: all clean examples demos check

all: examples demos

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

example_mesh_topology: example_mesh_topology.c noc_topology.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

example_vc_wormhole: example_vc_wormhole.c vc_wormhole.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

example_axi_noc_bridge: example_axi_noc_bridge.c axi_protocol.o noc_topology.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

demo_noc_simulation: demo_noc_simulation.c $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

demo_router_pipeline: demo_router_pipeline.c $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

examples: $(EXAMPLES)

demos: $(DEMOS)

check: all
	@echo === Running examples ===
	./example_mesh_topology
	./example_vc_wormhole
	./example_axi_noc_bridge
	@echo === Running demos ===
	./demo_noc_simulation
	./demo_router_pipeline
	@echo === All checks passed ===

clean:
	-del /q *.o *.exe 2>nul || rm -f *.o *.exe example_mesh_topology example_vc_wormhole example_axi_noc_bridge demo_noc_simulation demo_router_pipeline

rebuild: clean all
