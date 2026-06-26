# API Reference

## Verilog Simulator (`verilog_sim.h`)

| Function | Description |
|---|---|
| `vs_init(VerilogSimulator *sim)` | Initialize a Verilog simulator instance. |
| `vs_add_module(VerilogSimulator *sim, const char *name)` | Create a new module, returns module index. |
| `vs_get_module(VerilogSimulator *sim, int idx)` | Get pointer to a module by index. |
| `vs_add_port(VerilogModule *mod, const char *name, VerilogPortDir dir, int width)` | Add a port (INPUT/OUTPUT/INOUT) to a module. |
| `vs_add_net(VerilogModule *mod, const char *name, VerilogNetKind kind, int width)` | Add a net (WIRE/REG/WAND/etc.) to a module. |
| `vs_add_assign(VerilogModule *mod, int lhs_net, const VerilogValue *rhs, int width)` | Add a continuous assignment (`assign`). |
| `vs_add_always(VerilogModule *mod)` | Add an always block, returns block index. |
| `vs_add_sensitivity(VerilogAlwaysBlock *blk, VerilogSensitivityKind kind, int signal)` | Add sensitivity (POSEDGE/NEGEDGE/LEVEL) to an always block. |
| `vs_add_stmt(VerilogAlwaysBlock *blk, VerilogStmtKind kind)` | Add a statement to an always block. |
| `vs_add_blocking_assign(VerilogAlwaysBlock *blk, int stmt, int lhs, const VerilogValue *rhs, int width)` | Set up a blocking assignment (`=`). |
| `vs_add_nonblocking_assign(VerilogAlwaysBlock *blk, int stmt, int lhs, const VerilogValue *rhs, int width, int delay)` | Set up a non-blocking assignment (`<=`) with delay. |
| `vs_schedule_event(VerilogSimulator *sim, VerilogTime t, VerilogEventType type, int signal, int module)` | Schedule an event on the event queue. |
| `vs_evaluate_continuous_assigns(VerilogModule *mod)` | Evaluate all continuous assignments for a module. |
| `vs_evaluate_always_block(VerilogSimulator *sim, VerilogModule *mod, VerilogAlwaysBlock *blk)` | Evaluate an always block's statements. |
| `vs_run(VerilogSimulator *sim, VerilogTime end_time)` | Run event-driven simulation until end_time. |
| `vs_set_net_value(VerilogModule *mod, int net, VerilogValue val)` | Set a net's value (0/1/X/Z). |
| `vs_get_net_value(const VerilogModule *mod, int net)` | Get a net's current value. |
| `vs_display_signals(const VerilogModule *mod, VerilogTime t)` | Print all signal values for a module. |
| `vs_vcd_open(VerilogSimulator *sim, const char *filename)` | Open a VCD file for waveform output. |
| `vs_vcd_dump_signals(VerilogSimulator *sim)` | Write current signal values to VCD. |
| `vs_vcd_print_time(VerilogSimulator *sim, VerilogTime t)` | Write a timestamp to VCD. |
| `vs_vcd_close(VerilogSimulator *sim)` | Close the VCD file. |
| `vs_free(VerilogSimulator *sim)` | Free all allocated resources. |

## VHDL Simulator (`vhdl_sim.h`)

| Function | Description |
|---|---|
| `vhdl_init(VhdlSimulator *sim)` | Initialize a VHDL simulator instance. |
| `vhdl_add_entity(VhdlSimulator *sim, const char *name)` | Create a new entity. |
| `vhdl_get_entity(VhdlSimulator *sim, int idx)` | Get entity by index. |
| `vhdl_add_port(VhdlEntity *ent, const char *name, VhdlPortMode mode, VhdlSignalType type, int width)` | Add a port (IN/OUT/INOUT/BUFFER). |
| `vhdl_add_architecture(VhdlSimulator *sim, int entity_idx, const char *name)` | Create architecture for an entity. |
| `vhdl_add_signal(VhdlArchitecture *arch, const char *name, VhdlSignalType type, int width, bool resolved)` | Add a signal (optionally resolved for std_logic). |
| `vhdl_add_variable(VhdlProcess *proc, const char *name, VhdlSignalType type, int width)` | Add a variable to a process. |
| `vhdl_add_process(VhdlArchitecture *arch, const char *name)` | Add a process with optional sensitivity. |
| `vhdl_add_process_sensitivity(VhdlProcess *proc, int signal_idx)` | Add a sensitivity entry to a process. |
| `vhdl_add_process_stmt(VhdlProcess *proc, VhdlStmtKind kind)` | Add a statement to a process. |
| `vhdl_add_signal_assign(VhdlProcess *proc, int stmt, int lhs, const VhdlStdLogic *rhs, int width, int delay)` | Configure a signal assignment statement. |
| `vhdl_add_concurrent(VhdlArchitecture *arch, VhdlConcurrentKind kind)` | Add a concurrent statement. |
| `vhdl_resolve_std_logic(const VhdlStdLogic *drivers, int count)` | Resolve multiple drivers to a single std_logic value. |
| `vhdl_run_delta_cycle(VhdlSimulator *sim, VhdlArchitecture *arch)` | Execute one delta cycle (may run multiple internal deltas). |
| `vhdl_evaluate_process(VhdlSimulator *sim, VhdlArchitecture *arch, VhdlProcess *proc)` | Evaluate a single process. |
| `vhdl_evaluate_concurrent(VhdlArchitecture *arch)` | Evaluate all concurrent statements. |
| `vhdl_resolve_signals(VhdlArchitecture *arch)` | Run resolution functions on resolved signals. |
| `vhdl_run(VhdlSimulator *sim, uint64_t end_time)` | Run simulation until end_time. |
| `vhdl_display_signals(const VhdlArchitecture *arch)` | Print all signal values. |
| `vhdl_free(VhdlSimulator *sim)` | Free all allocated resources. |

## SystemVerilog Simulator (`systemverilog_sim.h`)

| Function | Description |
|---|---|
| `sv_init(SvSimulator *sim)` | Initialize a SystemVerilog simulator. |
| `sv_add_module(SvSimulator *sim, const char *name)` | Create a new module. |
| `sv_add_port(SvModule *mod, const char *name, SvPortDir dir, int width)` | Add a port. |
| `sv_add_signal(SvModule *mod, const char *name, int width, bool four_state)` | Add a logic signal (4-state or 2-state). |
| `sv_add_always_ff(SvModule *mod, const char *clk_signal, bool posedge)` | Add an `always_ff` block triggered by clock edge. |
| `sv_add_always_comb(SvModule *mod)` | Add an `always_comb` block (auto-sensitivity). |
| `sv_add_interface(SvModule *mod, const char *name)` | Add an interface construct. |
| `sv_add_enum(SvModule *mod, const char *name)` | Add an enumerated type. |
| `sv_add_enum_member(SvEnum *e, const char *name, int value)` | Add a member to an enum. |
| `sv_add_struct(SvModule *mod, const char *name)` | Add a struct type. |
| `sv_add_struct_field(SvStruct *s, const char *name, int width, bool is_signed)` | Add a field to a struct. |
| `sv_add_package(SvSimulator *sim, const char *name)` | Add a package. |
| `sv_add_package_item(SvPackage *pkg, const char *name, int value, int type)` | Add an item to a package. |
| `sv_add_assertion(SvModule *mod, const char *cond, SvAssertKind kind)` | Add an immediate or concurrent assertion. |
| `sv_add_stmt(SvAlwaysBlock *blk, SvStmtKind kind)` | Add a statement to an always block. |
| `sv_add_assign(SvAlwaysBlock *blk, int stmt, int lhs, const SvLogicValue *rhs, int width)` | Add an assignment statement. |
| `sv_evaluate_always_ff(SvModule *mod, SvAlwaysBlock *blk)` | Evaluate flip-flop always block. |
| `sv_evaluate_always_comb(SvModule *mod, SvAlwaysBlock *blk)` | Evaluate combinational always block (iterates to stability). |
| `sv_evaluate_assertions(SvModule *mod)` | Evaluate all immediate assertions. |
| `sv_run(SvSimulator *sim, uint64_t end_time)` | Run simulation. |
| `sv_set_signal(SvModule *mod, int sig, SvLogicValue val)` | Set a signal value. |
| `sv_get_signal(const SvModule *mod, int sig)` | Get a signal value. |
| `sv_display_module(const SvModule *mod)` | Display module state. |
| `sv_free(SvSimulator *sim)` | Free all resources. |

## Testbench Generator (`testbench_gen.h`)

| Function | Description |
|---|---|
| `tb_init(TbTestbench *tb, const char *name, uint64_t end_time)` | Initialize testbench. |
| `tb_add_clock(TbTestbench *tb, const char *name, int period, int duty_pct)` | Add a clock signal. |
| `tb_set_clock_mode(TbClock *clk, TbClockMode mode)` | Set clock mode (FREE_RUNNING/GATED/DIVIDED). |
| `tb_add_reset(TbTestbench *tb, const char *name, int signal_idx, TbResetPolarity pol)` | Add a reset signal. |
| `tb_set_reset_timing(TbReset *rst, int active_dur, int deassert_t)` | Configure reset timing. |
| `tb_add_stimulus(TbTestbench *tb, const char *name, int signal_idx, TbStimulusType type)` | Add stimulus to a signal. |
| `tb_stim_set(TbStimulus *stim, int value, int width)` | Configure set-type stimulus. |
| `tb_stim_pulse(TbStimulus *stim, int high_val, int low_val, int width, int high_dur, int low_dur)` | Configure pulse stimulus. |
| `tb_stim_random(TbStimulus *stim, int min_val, int max_val, int width, int seed)` | Configure random stimulus. |
| `tb_stim_increment(TbStimulus *stim, int start_val, int incr)` | Configure incrementing stimulus. |
| `tb_set_repeat(TbStimulus *stim, int count, int interval)` | Set stimulus repeat parameters. |
| `tb_add_monitor(TbTestbench *tb, const char *name, int signal_idx, TbMonitorType type)` | Add a signal monitor. |
| `tb_monitor_set_expected(TbMonitor *mon, int expected)` | Set expected value for assert monitor. |
| `tb_monitor_set_display_fmt(TbMonitor *mon, const char *fmt)` | Set display format string. |
| `tb_add_coverage_point(TbTestbench *tb, const char *name, int signal_idx)` | Add a coverage collection point. |
| `tb_coverage_add_bin(TbCoveragePoint *cp, int low, int high)` | Add a coverage bin. |
| `tb_evaluate_clock(TbTestbench *tb, TbClock *clk)` | Evaluate clock at current time. |
| `tb_evaluate_reset(TbTestbench *tb, TbReset *rst)` | Evaluate reset at current time. |
| `tb_evaluate_stimulus(TbTestbench *tb, TbStimulus *stim)` | Evaluate stimulus at current time. |
| `tb_evaluate_monitor(TbTestbench *tb, TbMonitor *mon, int actual_value)` | Evaluate a monitor, returns check result. |
| `tb_update_coverage(TbTestbench *tb, TbCoveragePoint *cp, int value)` | Update coverage bin hits. |
| `tb_record_waveform_sample(TbTestbench *tb, int signal_idx, int value)` | Record a waveform sample. |
| `tb_run(TbTestbench *tb)` | Run testbench for configured duration. |
| `tb_report(TbTestbench *tb)` | Print testbench pass/fail report. |
| `tb_dump_waveform_vcd(TbTestbench *tb, const char *filename)` | Write waveform to VCD file. |
| `tb_print_coverage(TbTestbench *tb)` | Print coverage statistics. |
| `tb_free(TbTestbench *tb)` | Free all resources. |

## Waveform Viewer (`waveform_view.h`)

| Function | Description |
|---|---|
| `wv_init(WvVcdData *vcd)` | Initialize VCD data structure. |
| `wv_set_timescale(WvVcdData *vcd, int magnitude, WvTimeUnit unit)` | Set timescale for time display. |
| `wv_parse_vcd(WvVcdData *vcd, const char *filename)` | Parse a VCD file (header + value changes). |
| `wv_parse_vcd_header(WvVcdData *vcd)` | Parse only the VCD header/definitions section. |
| `wv_parse_vcd_changes(WvVcdData *vcd)` | Parse value change data from VCD body. |
| `wv_add_scope(WvVcdData *vcd, const char *name, const char *type, int parent)` | Add a hierarchy scope. |
| `wv_add_signal(WvVcdData *vcd, const char *name, char id, int width, WvVarType type, int scope)` | Register a signal from VCD. |
| `wv_add_value_change(WvVcdData *vcd, int signal_idx, uint64_t time, const char *value)` | Record a value change for a signal. |
| `wv_find_signal_by_id(WvVcdData *vcd, char id)` | Find signal by VCD identifier character. |
| `wv_find_signal_by_name(WvVcdData *vcd, const char *name)` | Find signal by name. |
| `wv_detect_transitions(WvVcdData *vcd)` | Analyze all signals and detect transition types. |
| `wv_get_signal_value_at_time(const WvSignal *sig, uint64_t time, char *buf, int buf_size)` | Get a signal's value at a specific time. |
| `wv_get_signal_range(const WvSignal *sig, uint64_t start, uint64_t end, WvValueChange **out_changes, int *out_count)` | Get value changes within a time window. |
| `wv_init_viewer(WvViewer *viewer)` | Initialize the ASCII waveform viewer. |
| `wv_set_view_range(WvViewer *viewer, uint64_t start, uint64_t end)` | Set the visible time range. |
| `wv_render_ascii_waveform(const WvVcdData *vcd, const WvViewer *viewer, FILE *out)` | Render signals as ASCII art waveforms. |
| `wv_render_signal_trace(const WvSignal *sig, uint64_t start, uint64_t end, int display_width, char *buf, int buf_size)` | Render one signal's trace. |
| `wv_print_signal_list(const WvVcdData *vcd, FILE *out)` | Print list of all parsed signals. |
| `wv_print_time_info(const WvVcdData *vcd, uint64_t time, FILE *out)` | Print signal values at a specific time. |
| `wv_time_unit_name(WvTimeUnit unit)` | Get timescale unit name string. |
| `wv_transition_kind_name(WvTransitionKind kind)` | Get transition type name string. |
| `wv_free(WvVcdData *vcd)` | Free VCD data resources. |
| `wv_free_viewer(WvViewer *viewer)` | Free viewer resources. |
