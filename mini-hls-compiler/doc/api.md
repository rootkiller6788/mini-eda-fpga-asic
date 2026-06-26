# API Reference: mini-hls-compiler

## Module 1: Pipeline (`hls_pipeline.h`)

### DFG Construction

```c
HlsDataFlowGraph* hls_dfg_create(void);
void hls_dfg_destroy(HlsDataFlowGraph *dfg);
```
Create/destroy a Data Flow Graph. On creation, allocates space for 1024 nodes
and 256 basic blocks.

```c
HlsNode* hls_dfg_add_node(HlsDataFlowGraph *dfg, HlsOpType op, const char *name);
```
Add a node to the DFG. Returns the node pointer, or NULL on failure. Default
latency is 3 for MUL/DIV, 1 for other ops. Sets `bound_unit` to -1 (unbound).

```c
void hls_dfg_add_edge(HlsNode *src, HlsNode *dst);
```
Add a directed edge from src to dst. Each node supports up to 8 inputs and 8
outputs.

```c
HlsBasicBlock* hls_dfg_add_block(HlsDataFlowGraph *dfg);
void hls_dfg_add_edge_bb(HlsBasicBlock *src, HlsBasicBlock *dst);
```
Create CFG basic blocks and connect them with predecessor/successor edges.

### Scheduling

```c
HlsScheduleResult hls_schedule_asap(HlsDataFlowGraph *dfg);
```
As-Soon-As-Possible scheduling. Forward topological pass. Sets `asap_time`,
`scheduled_cycle`, and `is_scheduled` on each node. Returns `total_cycles`
and `critical_path`.

```c
HlsScheduleResult hls_schedule_alap(HlsDataFlowGraph *dfg, uint32_t max_cycles);
```
As-Late-As-Possible scheduling. Iterative reverse pass until convergence.
Sets `alap_time` on each node. `max_cycles` is the upper bound (typically
from ASAP result).

```c
HlsScheduleResult hls_schedule_list(HlsDataFlowGraph *dfg, uint32_t *res_limits);
```
Resource-constrained list scheduling. `res_limits[8]` specifies the limit
for each resource type: `[ALU, MUL, DIV, MEM_RD, MEM_WR, BRAM, DSP, LUT, FF]`.

### Binding

```c
bool hls_bind_resources(HlsDataFlowGraph *dfg, HlsScheduleResult *sched);
```
Greedy resource binding. Maps each node's operation to a resource type index.
Sets `bound_unit` on each node.

### Pipeline

```c
HlsPipelineConfig hls_pipeline_create(uint32_t ii, uint32_t num_stages);
```
Create a pipeline configuration with given Initiation Interval and stage count.
Allocates stage descriptors. Default: stallable, latency=1 per stage.

```c
void hls_pipeline_stall_insert(HlsPipelineConfig *cfg, uint32_t stage);
```
Mark a specific stage as stallable, enabling the stall capability globally.

```c
void hls_pipeline_flush_enable(HlsPipelineConfig *cfg, bool en);
```
Enable/disable flush on all stages.

```c
bool hls_pipeline_verify(HlsPipelineConfig *cfg, HlsScheduleResult *sched);
```
Verify that a pipeline configuration is consistent with the schedule result.

### RTL Generation

```c
HlsRTLModule* hls_generate_rtl(HlsDataFlowGraph *dfg, HlsPipelineConfig *cfg);
```
Generate an RTL module from the DFG and pipeline configuration. Returns a
`HlsRTLModule` with pipeline copy, latency, and throughput information.

```c
void hls_rtl_destroy(HlsRTLModule *mod);
void hls_rtl_print_verilog(HlsRTLModule *mod, FILE *out);
```
Destroy and print the RTL module as synthesizable Verilog with pipeline
registers and reset logic.

### Stall/Flush

```c
void hls_stall_condition_set(HlsPipelineConfig *cfg, uint32_t stage,
       bool (*cond)(void*), void *ctx);
void hls_flush_trigger_set(HlsPipelineConfig *cfg,
       bool (*cond)(void*), void *ctx);
bool hls_pipeline_is_stalled(HlsPipelineConfig *cfg);
void hls_pipeline_advance(HlsPipelineConfig *cfg, uint32_t cycles);
```
Runtime stall/flush control for simulation.

---

## Module 2: Loop Optimization (`loop_optimize.h`)

### Nest Management

```c
HlsLoopNest* hls_loop_nest_create(void);
void         hls_loop_nest_destroy(HlsLoopNest *nest);
```
Create/destroy a loop nest. Allocates space for 256 loops.

```c
HlsLoop* hls_loop_add(HlsLoopNest *nest, HlsLoop *parent,
         LoopType type, const char *label);
void     hls_loop_set_trip_count(HlsLoop *loop, int64_t count);
```
Add a loop to the nest. Sets parent, depth, and registers with parent's
children list. Up to 16 children per loop.

### Unroll

```c
bool hls_loop_unroll_partial(HlsLoop *loop, uint32_t factor);
bool hls_loop_unroll_complete(HlsLoop *loop);
bool hls_loop_unroll_configured(HlsLoop *loop, const HlsUnrollConfig *cfg);
bool hls_loop_unroll_is_legal(HlsLoop *loop, uint32_t factor);
```
Partial unroll divides trip count by factor. Complete unroll (factor=0)
requires constant trip count. `is_legal` checks trip count divisibility.

### Flatten

```c
bool hls_loop_flatten(HlsLoopNest *nest, const HlsFlattenConfig *cfg);
bool hls_loop_flatten_pair(HlsLoop *outer, HlsLoop *inner);
```
`flatten` processes all qualifying loops. `flatten_pair` collapses an inner
loop into its parent, multiplying trip counts. Maximum depth after flattening
is configurable.

### Merge

```c
bool     hls_loop_merge(HlsLoop *loop1, HlsLoop *loop2);
uint32_t hls_loop_merge_consecutive(HlsLoopNest *nest, const HlsMergeConfig *cfg);
```
Merges two sibling loops with identical trip counts and types. `merge_consecutive`
scans for mergeable adjacent loops with configurable max gap.

### Pipeline Rewind

```c
bool hls_loop_rewind_configure(HlsLoop *loop, const HlsRewindConfig *cfg);
bool hls_loop_rewind_enable(HlsLoop *loop, bool en);
void hls_loop_rewind_reset(HlsLoop *loop);
```
Enables continuous pipeline rewind with partial depth control. When enabled,
sets pipeline_ii to 1 if previously 0.

### Trip Count

```c
HlsTripCount hls_analyze_trip_count(HlsLoop *loop);
int64_t      hls_trip_count_ceil_div(int64_t n, int64_t d);
bool         hls_trip_count_can_compute(HlsLoop *loop);
```
Analyzes loop bounds. Returns min/max/avg trips, constant/affine flags.
`can_compute` returns true only for constant trip counts.

### Pipeline

```c
bool hls_loop_pipeline_set_ii(HlsLoop *loop, uint32_t ii);
bool hls_loop_pipeline_verify(HlsLoopNest *nest);
void hls_loop_pipeline_print(HlsLoopNest *nest, FILE *out);
```
Sets desired II on a loop. Verify checks all pipelined loops have non-zero II.
Print outputs a formatted table of loop properties.

---

## Module 3: Dataflow (`dataflow_opt.h`)

### Graph Management

```c
HlsDataflowGraph* hls_dataflow_create(const char *name);
void     hls_dataflow_destroy(HlsDataflowGraph *df);
HlsTask*  hls_dataflow_add_task(HlsDataflowGraph *df, const char *name);
HlsChannel* hls_dataflow_add_channel(HlsDataflowGraph *df,
              HlsChannelType type, const char *name);
bool     hls_dataflow_connect(HlsTask *producer, HlsChannel *ch,
           HlsTask *consumer);
```
Creates a dataflow graph with up to 64 tasks and 64 channels. `connect` links
a producer task to a channel to a consumer task, incrementing producer/consumer
counts.

### FIFO Channel

```c
bool hls_channel_fifo_configure(HlsChannel *ch, uint32_t depth, uint32_t width);
bool hls_channel_fifo_push(HlsChannel *ch, const void *data, uint32_t size);
bool hls_channel_fifo_pop(HlsChannel *ch, void *data, uint32_t size);
bool hls_channel_fifo_full(HlsChannel *ch);
bool hls_channel_fifo_empty(HlsChannel *ch);
```
Configures FIFO depth and width. Push/pop model FIFO operations. Full/empty
status queries for flow control.

### Ping-Pong Buffer

```c
bool  hls_channel_pingpong_configure(HlsChannel *ch, uint32_t buf_size);
bool  hls_channel_pingpong_swap(HlsChannel *ch);
void* hls_channel_pingpong_write_buf(HlsChannel *ch);
void* hls_channel_pingpong_read_buf(HlsChannel *ch);
```
Configures a 2-buffer ping-pong scheme. `swap` toggles active buffers. Returns
pointer to current write/read buffer.

### Streaming Interface

```c
bool hls_stream_create(HlsChannel *ch, uint32_t depth, uint32_t width);
bool hls_stream_write(HlsChannel *ch, const void *data);
bool hls_stream_read(HlsChannel *ch, void *data);
bool hls_stream_is_full(HlsChannel *ch);
bool hls_stream_is_empty(HlsChannel *ch);
```
hls::stream-compatible streaming interface. Write/read with full/empty
backpressure. Initial state: empty=true, full=false.

### Stencil

```c
bool hls_stencil_define(HlsTask *task, HlsStencilDim dim,
       const int32_t radius[3]);
bool hls_stencil_line_buffer(HlsTask *task, uint32_t rows);
bool hls_stencil_validate(HlsTask *task);
```
Defines a stencil window. Computes window_size as product of (2*radius+1)
across dimensions. For 2D/3D, `line_buffer_depth` sets row buffering.
`validate` checks window size > 0 and line buffer is set for 2D+.

### Analysis

```c
bool hls_dataflow_schedule(HlsFlowGraph *df);
bool hls_dataflow_balance(HlsDataflowGraph *df);
bool hls_dataflow_verify(HlsDataflowGraph *df);
void hls_dataflow_report(HlsDataflowGraph *df, FILE *out);
```
Schedule links tasks in linear order. Balance finds max latency task.
Verify checks all channels are connected. Report prints task and channel
details.

---

## Module 4: Array Partition (`array_partition.h`)

### Array Management

```c
HlsArray* hls_array_create(const char *name, uint32_t elem_width, uint32_t num_dims);
void      hls_array_destroy(HlsArray *arr);
bool      hls_array_set_dim(HlsArray *arr, uint32_t dim, uint32_t size);
```
Creates an array descriptor. Up to 8 dimensions. `set_dim` configures size
per dimension and recalculates `total_elements`.

### Partition

```c
bool hls_array_partition_block(HlsArray *arr, uint32_t dim, uint32_t factor);
bool hls_array_partition_cyclic(HlsArray *arr, uint32_t dim, uint32_t factor);
bool hls_array_partition_complete(HlsArray *arr, uint32_t dim);
bool hls_array_partition_configured(HlsArray *arr, const HlsPartConfig *cfg);
bool hls_array_partition_legal(HlsArray *arr, const HlsPartConfig *cfg);
```
Block partition: contiguous chunks. Requires size % factor == 0.
Cyclic partition: round-robin. Complete: factor = size.
`legal` checks block divisibility.

### Reshape

```c
bool hls_array_reshape(HlsArray *arr, const HlsReshapeConfig *cfg);
bool hls_array_reshape_block(HlsArray *arr, uint32_t dim, uint32_t factor);
bool hls_array_reshape_cyclic(HlsArray *arr, uint32_t dim, uint32_t factor);
```
Reshape combines elements to widen data width. Block reshape divides
dimension size by factor and multiplies elem_width.

### Memory Banking

```c
bool     hls_array_bank_analyze(HlsArray *arr, HlsBankingReport *report);
bool     hls_array_bank_assign(HlsArray *arr, uint32_t num_banks);
uint32_t hls_array_get_bank(HlsArray *arr, uint32_t elem_index);
bool     hls_array_bank_conflict_detect(HlsArray *arr,
             const uint32_t *access_indices, uint32_t num_accesses);
```
Bank analysis reports depth, ports, and conflicts. `get_bank` maps element
index to bank (modulo num_banks). Conflict detection checks if requested
accesses exceed unique bank count, incrementing `bank_conflicts` counter.

### Memory Type

```c
HlsMemoryTradeoff hls_memory_tradeoff_analyze(uint32_t elems, uint32_t width);
bool    hls_array_set_memory_type(HlsArray *arr, HlsMemType type);
HlsMemType hls_array_recommend_memory(HlsArray *arr);
```
Tradeoff analysis uses threshold: >= 128 elements -> BRAM, <= 256 -> LUTRAM.
Calculates BRAM count (16Kbit blocks), LUT usage (64 bits per LUT), and
FF usage.

### Port Management

```c
bool hls_array_set_ports(HlsArray *arr, uint32_t num_ports, bool dual_port);
bool hls_array_can_access_parallel(HlsArray *arr, uint32_t num_accesses);
```
Configures port count (1-4) and dual-port flag. `can_access_parallel` checks
if num_accesses <= num_banks * num_ports.

---

## Module 5: Interface & Pragma (`interface_pragma.h`)

### Pragma Set

```c
HlsPragmaSet* hls_pragma_set_create(void);
void          hls_pragma_set_destroy(HlsPragmaSet *ps);
void          hls_pragma_set_top(HlsPragmaSet *ps, const char *func_name);
void          hls_pragma_set_clock(HlsPragmaSet *ps, uint32_t period_ns);
bool          hls_pragma_validate(HlsPragmaSet *ps);
```
Allocates a pragma set with arrays for interfaces (32), unrolls (16),
array_parts (16), and resources (16). Default: 10ns clock, active-low reset.
Validate checks top function, clock period, and directive consistency.

### Interface Assignment

```c
void hls_interface_set(HlsPragmaSet *ps, const char *port, HlsInterfaceType type);
void hls_interface_bundle(HlsPragmaSet *ps, const char *bundle_name,
       const char **ports, uint32_t count, HlsInterfaceType type);
void hls_interface_axi_master(HlsPragmaSet *ps, const char *port,
       const HlsAxiMasterConfig *cfg);
void hls_interface_axi_stream(HlsPragmaSet *ps, const char *port, uint32_t depth);
void hls_interface_s_axilite(HlsPragmaSet *ps, const char *port);
void hls_interface_ap_ctrl_none(HlsPragmaSet *ps);
```

### AXI4-Stream Handshake

```c
bool hls_axis_handshake(HlsAxisSignals *sig);
void hls_axis_valid_set(HlsAxisSignals *sig, bool v);
bool hls_axis_ready_get(const HlsAxisSignals *sig);
void hls_axis_signal_init(HlsAxisSignals *sig);
bool hls_axis_transfer_done(const HlsAxisSignals *sig);
```
Handshake = valid && ready. Init sets tkeep/tstrb to 0xFF. Transfer done
requires valid, ready, AND tlast.

### AXI4-Master

```c
void hls_axi_master_config_default(HlsAxiMasterConfig *cfg);
bool hls_axi_burst_read(HlsAxiMasterConfig *cfg, uint64_t addr,
       uint32_t len, void *buf);
bool hls_axi_burst_write(HlsAxiMasterConfig *cfg, uint64_t addr,
       uint32_t len, const void *buf);
```
Defaults: burst_len=16, burst_size=4B, 1 read/write channel.
Burst read/write validates length against max_burst_len.

### Bundle

```c
HlsBundle* hls_bundle_create(const char *name, HlsInterfaceType type);
void       hls_bundle_destroy(HlsBundle *b);
bool       hls_bundle_add_port(HlsBundle *b, const char *port);
```
Groups up to 16 ports under a named bundle sharing an interface type.

### Directives

```c
void hls_pragma_pipeline(HlsPragmaSet *ps, uint32_t ii, bool flush, bool rewind);
void hls_pragma_pipeline_set_target(HlsPragmaSet *ps, const char *target);
void hls_pragma_unroll(HlsPragmaSet *ps, uint32_t factor, const char *region);
void hls_pragma_array_partition(HlsPragmaSet *ps, const char *var,
       HlsArrayPartType type, uint32_t dim, uint32_t factor);
void hls_pragma_dataflow(HlsPragmaSet *ps, const char *region);
void hls_pragma_resource(HlsPragmaSet *ps, const char *op,
       const char *core, int32_t limit);
```

### Output

```c
void hls_pragma_print_tcl(HlsPragmaSet *ps, FILE *out);
void hls_pragma_print_directives(HlsPragmaSet *ps, FILE *out);
```
`print_tcl` generates a complete Vivado HLS Tcl script with project setup,
interface directives, pipeline/unroll/array_partition/dataflow/resource
directives, synthesis, and export commands. `print_directives` outputs a
human-readable summary.

---

## Enumerations

| Enum | Values |
|------|--------|
| `HlsOpType` | ADD, SUB, MUL, DIV, AND, OR, XOR, NOT, SHL, SHR, SEL, CMP, LD, ST, PHI, CALL, RET, BR, CONST, NOP |
| `HlsSchedAlgo` | ASAP, ALAP, LIST, FORCE_DIRECTED |
| `HlsResType` | ALU, MUL, DIV, MEM_RD, MEM_WR, BRAM, DSP, LUT, FF |
| `LoopType` | FOR, WHILE, DO_WHILE |
| `UnrollKind` | NONE, PARTIAL, COMPLETE |
| `HlsTaskState` | IDLE, RUNNING, STALLED, DONE |
| `HlsChannelType` | FIFO, PINGPONG, STREAM, SHARED_MEM |
| `HlsStencilDim` | 1D, 2D, 3D |
| `HlsArrayPartType` | NONE, BLOCK, CYCLIC, COMPLETE |
| `HlsMemType` | BRAM, LUTRAM, URAM, DISTRIBUTED, AUTO |
| `HlsInterfaceType` | AP_CTRL_NONE, AP_CTRL_HS, AP_CTRL_CHAIN, S_AXILITE, M_AXI, AXIS, AP_FIFO, AP_MEMORY, AP_BUS, AP_NONE, AP_STABLE, AP_VLD |

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `HLS_MAX_NODES` | 1024 | Max DFG nodes |
| `HLS_MAX_BLOCKS` | 256 | Max basic blocks |
| `HLS_MAX_LOOPS` | 256 | Max loops in nest |
| `HLS_MAX_CHILDREN` | 16 | Max children per loop |
| `HLS_MAX_TASKS` | 64 | Max dataflow tasks |
| `HLS_MAX_CHANNELS` | 64 | Max dataflow channels |
| `HLS_MAX_DIMS` | 8 | Max array dimensions |
| `HLS_BRAM_THRESH` | 128 | Min elements for BRAM |
| `HLS_MAX_BANKS` | 16 | Max memory banks |
