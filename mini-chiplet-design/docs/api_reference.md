# mini-chiplet-design API Reference

## ucie_d2d.h — UCIe Die-to-Die Interface

### Types

```c
typedef enum { UCIE_LINK_DOWN, UCIE_LINK_TRAINING, UCIE_LINK_ACTIVE,
               UCIE_LINK_ERROR, UCIE_LINK_RECOVERY } ucie_link_state_t;

typedef enum { UCIE_PROTO_RAW, UCIE_PROTO_CXL_MEM, UCIE_PROTO_CXL_CACHE,
               UCIE_PROTO_PCIE, UCIE_PROTO_STREAMING } ucie_protocol_t;

typedef struct { ... } ucie_flit_t;
typedef struct { ... } ucie_phy_config_t;
typedef struct { ... } ucie_lane_status_t;
typedef struct { ... } ucie_link_t;
typedef struct { ... } ucie_callbacks_t;
```

### Functions

| Function | Description |
|----------|-------------|
| `ucie_init()` | Initialize UCIe link with lane count and GT/s |
| `ucie_phy_init()` | Configure PHY parameters |
| `ucie_link_train()` | Run link training sequence |
| `ucie_lane_deskew()` | Deskew all lanes |
| `ucie_send_flit()` | Send a flit (increment send counter) |
| `ucie_recv_flit()` | Receive and verify a flit |
| `ucie_calc_bandwidth()` | Calculate effective bandwidth in Gbps |
| `ucie_measure_ber()` | Average BER across all lanes |
| `ucie_link_recovery()` | Recover link from error state |
| `ucie_reset()` | Reset link to DOWN state |
| `ucie_flit_crc32()` | Compute CRC32 for a flit |
| `ucie_flit_pack()` | Pack data into a flit with CRC |
| `ucie_flit_verify()` | Verify flit CRC |
| `ucie_print_link_status()` | Print link status |
| `ucie_dump_flit()` | Print flit contents |

### Constants

| Symbol | Value | Description |
|--------|-------|-------------|
| UCIE_MAX_LANES | 64 | Max lanes per link |
| UCIE_GT_MODE_16 | 0 | 16 GT/s mode |
| UCIE_GT_MODE_24 | 1 | 24 GT/s mode |
| UCIE_GT_MODE_32 | 2 | 32 GT/s mode |
| UCIE_FLIT_SIZE | 256 | Flit size in bits |
| UCIE_BER_THRESHOLD | 1e-27 | BER spec |
| UCIE_MAX_LATENCY_PS | 2000 | Max latency in ps |

---

## interposer_tech.h — Interposer Technology

### Types

```c
typedef enum { INTERPOSER_SILICON, INTERPOSER_ORGANIC_EMIB,
               INTERPOSER_GLASS, INTERPOSER_SILICON_BRIDGE,
               INTERPOSER_RDL_FANOUT } interposer_type_t;

typedef struct { ... } die_geometry_t;
typedef struct { ... } microbump_array_t;
typedef struct { ... } tsv_site_t;
typedef struct { ... } rdl_trace_t;
typedef struct { ... } interposer_spec_t;
typedef struct { ... } interposer_t;
```

### Functions

| Function | Description |
|----------|-------------|
| `interposer_init()` | Initialize interposer with type and dimensions |
| `interposer_place_die()` | Place a die on the interposer (collision check) |
| `interposer_remove_die()` | Remove a die by index |
| `interposer_add_microbump()` | Add a microbump array |
| `interposer_add_tsv()` | Add a TSV site |
| `interposer_add_rdl_trace()` | Add an RDL trace |
| `interposer_calc_warpage()` | Calculate warpage in um given delta T |
| `interposer_calc_thermal_resistance()` | Calc interposer thermal resistance |
| `interposer_calc_ir_drop()` | Calculate IR drop for given current |
| `interposer_calc_signal_delay()` | Calculate signal delay for an RDL trace |
| `interposer_route_die_to_die()` | Auto-route signals between two dies |
| `interposer_optimize_placement()` | Grid-based placement optimization |
| `interposer_verify_drc()` | Run DRC checks |
| `interposer_print_summary()` | Print interposer summary |
| `emib_bridge_capacity_gbps()` | Estimate EMIB bridge capacity |

---

## mcm_integration.h — MCM Integration

### Types

```c
typedef enum { MCM_DIE_COMPUTE, MCM_DIE_HBM, MCM_DIE_IO,
               MCM_DIE_SERDES, MCM_DIE_ACCELERATOR,
               MCM_DIE_CXL_CONTROLLER } mcm_die_type_t;

typedef struct { ... } hbm_config_t;
typedef struct { ... } hbm_pseudo_channel_t;
typedef struct { ... } phy_macro_t;
typedef struct { ... } mcm_die_t;
typedef struct { ... } routing_channel_t;
typedef struct { ... } mcm_module_t;
```

### Functions

| Function | Description |
|----------|-------------|
| `mcm_init()` | Initialize MCM module with default HBM config |
| `mcm_add_die()` | Add a die to the module |
| `mcm_add_phy()` | Add a PHY macro to a die |
| `mcm_connect_phys()` | Connect two PHYs between different dies |
| `mcm_configure_hbm()` | Configure HBM parameters, init pseudo-channels |
| `mcm_route_hbm_to_compute()` | Route HBM pseudo-channels to compute die |
| `mcm_calc_total_bandwidth()` | Calculate aggregate bandwidth |
| `mcm_calc_hbm_efficiency()` | HBM utilization efficiency |
| `mcm_calc_power()` | Total module power estimate |
| `mcm_validate_connectivity()` | Check all dies have valid connections |
| `mcm_optimize_phy_placement()` | Place PHYs at die edges |
| `mcm_print_floorplan()` | Print die/PHY floorplan |
| `mcm_print_hbm_stats()` | Print HBM timing and capacity |
| `hbm_read_bandwidth()` | HBM read bandwidth at frequency |
| `hbm_write_bandwidth()` | HBM write bandwidth at frequency |
| `hbm_power_estimate()` | HBM power at utilization percentage |

---

## d2d_phy.h — Die-to-Die PHY

### Types

```c
typedef enum { D2D_CLK_FORWARDED, D2D_CLK_SOURCE_SYNC,
               D2D_CLK_EMBEDDED_SERDES, D2D_CLK_PLL_SYNCHRONIZED }
               d2d_clocking_mode_t;

typedef enum { PRBS7, PRBS9, PRBS15, PRBS23, PRBS31 } prbs_pattern_t;
typedef struct { ... } eye_diagram_t;
typedef struct { ... } d2d_phy_t;
```

### Functions

| Function | Description |
|----------|-------------|
| `d2d_phy_init()` | Initialize PHY with configuration |
| `d2d_tx_config()` | Configure TX (swing, emphasis, EQ) |
| `d2d_rx_config()` | Configure RX (vref, termination, EQ) |
| `d2d_training_sequence()` | Run training pattern detection |
| `d2d_deskew_lanes()` | Adjust per-lane delays |
| `d2d_equalization_train()` | Train TX/RX equalization |
| `d2d_cdr_lock()` | Check CDR lock status |
| `d2d_measure_eye()` | Measure eye diagram for a lane |
| `d2d_measure_ber()` | Measure BER for a lane |
| `d2d_measure_jitter()` | Measure total jitter for a lane |
| `prbs_generate()` | Generate PRBS pattern buffer |
| `prbs_verify()` | Verify PRBS pattern, count errors |
| `prbs_self_test()` | Self-test: generate and verify PRBS |
| `serdes_link_budget()` | SerDes link budget calculation |
| `serdes_ber_estimate()` | BER estimate from SNR |
| `eye_diagram_print()` | Print eye diagram |
| `d2d_phy_print_status()` | Print PHY status |
| `d2d_phy_print_lane()` | Print single lane status |

---

## thermal_power.h — Thermal & Power Delivery

### Types

```c
typedef enum { TIM_SOLDER, TIM_GREASE, TIM_PCM, TIM_LIQUID_METAL,
               TIM_GRAPHITE_PAD, TIM_SINTERED_SILVER } tim_material_t;

typedef struct { ... } thermal_resistance_t;
typedef struct { ... } hotspot_t;
typedef struct { ... } tim_spec_t;
typedef struct { ... } decap_t;
typedef struct { ... } thermal_state_t;
typedef struct { ... } power_delivery_t;
typedef struct { ... } thermal_power_model_t;
```

### Functions

| Function | Description |
|----------|-------------|
| `tp_init()` | Initialize thermal/power model |
| `tp_set_thermal_resistance()` | Set theta_JA and theta_JC |
| `tp_set_tim()` | Configure TIM material and thickness |
| `tp_add_hotspot()` | Add a hotspot region |
| `tp_remove_hotspot()` | Remove hotspot by index |
| `tp_calc_junction_temp()` | Calculate T_junction from power |
| `tp_calc_case_temp()` | Get T_case |
| `tp_calc_heatspreader_temp()` | Get heat spreader temperature |
| `tp_check_thermal_throttle()` | Check if throttling required |
| `tp_mitigate_hotspots()` | Spread hotspots to reduce peak temperature |
| `tp_pdn_init()` | Initialize power delivery network |
| `tp_pdn_ir_drop()` | Calculate IR drop |
| `tp_pdn_impedance_at_freq()` | PDN impedance at frequency |
| `tp_pdn_add_decap()` | Add a decoupling capacitor |
| `tp_pdn_resonance_freq()` | PDN resonance frequency |
| `tp_pdn_ripple()` | Voltage ripple estimate |
| `tp_calc_3d_stack_temp()` | 3D stack peak temperature |
| `tp_calc_cooling_required()` | Cooling capacity needed |
| `tim_select_conductivity()` | Get TIM thermal conductivity |
| `tim_material_name()` | Get TIM material display name |
| `tp_print_state()` | Print thermal state |
| `tp_print_hotspots()` | Print hotspots |
| `tp_print_pdn()` | Print PDN status |
