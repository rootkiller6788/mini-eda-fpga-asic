# mini-noc-design ? Knowledge Graph

## Nine-Level Knowledge Coverage

### L1: Definitions ? COMPLETE
- `noc_topology_t`: Topology graph (nodes, edges, metrics)
- `noc_node_t`, `noc_edge_t`: Graph elements
- `noc_flit_t`, `flit_type_t`: Flit/packet definitions
- `noc_router_t`, `noc_router_config_t`: Router microarchitecture
- `noc_vc_t`, `vc_state_t`: Virtual channel state machine
- `noc_crossbar_t`: Crossbar switch structure
- `noc_rr_arbiter_t`: Round-robin arbiter
- `noc_routing_func_t`: Routing function signature
- `noc_turn_table_t`: Turn restriction table
- `noc_channel_dep_graph_t`: Channel Dependency Graph
- `noc_routing_table_t`: Per-router routing table
- `noc_credit_channel_t`: Credit-based flow control
- `noc_buffer_pool_t`: Shared buffer pool
- `noc_wormhole_packet_t`: Packet tracking
- `noc_flow_controller_t`: Flow control module
- `noc_traffic_gen_t`: Traffic generator
- `noc_simulator_t`: Cycle-accurate simulator
- `noc_perf_metrics_t`: Performance metrics container
- `noc_qos_config_t`, `noc_qos_vc_allocator_t`: QoS structures
- `noc_wrr_arbiter_t`: Weighted Round-Robin
- `noc_rate_limiter_t`: Token bucket rate limiter
- `noc_sla_monitor_t`: SLA monitor
- `noc_deadlock_detector_t`: Deadlock detector
- `noc_wait_for_graph_t`: Wait-for graph
- `noc_cycle_result_t`: Cycle analysis result

### L2: Core Concepts ? COMPLETE
- 2D Mesh topology (k-ary 2-cube)
- 2D Torus topology (wraparound links)
- 1D Ring topology
- Fat-tree topology (k-ary n-tree)
- Butterfly network
- Wormhole flow control (flit-level pipelining)
- Virtual Channels (VCs) for deadlock avoidance
- Credit-based backpressure
- Dimension-Ordered Routing (DOR)
- Turn Model for deadlock-free routing
- Channel Dependency Graph (CDG)
- Priority-based QoS classes
- Best-effort vs guaranteed service

### L3: Engineering Structures ? COMPLETE
- 5-stage canonical router pipeline (RC?VA?SA?ST?LT)
- Input-buffered router architecture
- Separable allocator (VA + SA)
- Crossbar switch with grant logic
- VC state machine (IDLE?ROUTING?WAITING?ACTIVE?IDLE)
- Round-robin arbiter with priority pointer
- Floyd-Warshall all-pairs shortest paths
- Dijkstra shortest path with min-heap
- Deficit Weighted Round-Robin (DWRR)

### L4: Standards/Theorems ? COMPLETE
- **Dally & Towles (2004)**: k-ary n-cube topology properties
- **Dally (IEEE TPDS 1992)**: Virtual-Channel Flow Control
- **Dally & Seitz (IEEE TC 1987)**: Deadlock-Free Routing Theorem
- **Duato (IEEE TPDS 1993)**: Deadlock-Free Adaptive Routing
- **Glass & Ni (ISCA 1992)**: The Turn Model for Adaptive Routing
- **Peh & Dally (IEEE Micro 2001)**: Router Delay Model
- **Little's Law**: L = ??W (avg packets = arrival rate ? avg time)
- **Jain's Fairness Index (1984)**: J = (?xi)?/(n??xi?)
- **Dally (IEEE TC 1990)**: Hot-spot tree saturation model
- **Kahn (CACM 1962)**: Topological sorting algorithm
- **Tarjan (SIAM J. Comp. 1972)**: DFS cycle detection

### L5: Algorithms/Methods ? COMPLETE
- XY Routing (Dimension-Ordered)
- YX Routing
- West-First Routing (Turn Model)
- North-Last Routing (Turn Model)
- Negative-First Routing (Turn Model)
- Odd-Even Routing (Restricted Turn Model)
- Minimal Adaptive Routing
- Randomized Routing
- Floyd-Warshall all-pairs shortest path
- Dijkstra's algorithm for topology paths
- DFS cycle detection in CDG
- Kahn's topological sorting
- DWRR weighted fair queuing
- Token bucket rate limiting
- Bernoulli traffic injection

### L6: Canonical Problems ? COMPLETE
- Deadlock-free routing verification (CDG + cycle detection)
- Topology metric computation (diameter, bisection BW)
- Throughput-latency sweep simulation
- Hot-spot congestion analysis
- Turn model deadlock-freedom proof

### L7: Applications ? COMPLETE (4 examples)
- ex01: 2D Mesh XY routing path computation
- ex02: Deadlock-free routing verification across 8 algorithms
- ex03: Performance analysis with throughput-latency sweep
- ex04: QoS-aware routing with SLA monitoring
- demo01: Full cycle-accurate NoC simulation
- demo02: Topology comparison (mesh/torus/ring/tree/butterfly)

### L8: Advanced Topics ? Partial
- Deadlock recovery via escape VCs (Duato method)
- Adaptive routing with congestion awareness
- Weighted fairness arbitration

### L9: Industry Frontiers ? Partial (Documented)
- Intel Knights Landing: 2D mesh NoC (36 tiles)
- Tilera TILE64: 8x8 mesh NoC
- ST Microelectronics STNoC: AXI-compatible NoC IP
- Arteris FlexNoC: Commercial NoC IP
- Sonics SGN: SiliconBackplane NoC
- AMBA AXI/CHI: On-chip interconnect protocol
