#ifndef INTERFACE_PRAGMA_H
#define INTERFACE_PRAGMA_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "array_partition.h"

typedef enum {
    IF_AP_CTRL_NONE,
    IF_AP_CTRL_HS,
    IF_AP_CTRL_CHAIN,
    IF_S_AXILITE,
    IF_M_AXI,
    IF_AXIS,
    IF_AP_FIFO,
    IF_AP_MEMORY,
    IF_AP_BUS,
    IF_AP_NONE,
    IF_AP_STABLE,
    IF_AP_VLD
} HlsInterfaceType;

typedef struct {
    bool     tvalid;
    bool     tready;
    bool     tlast;
    uint8_t  tkeep;
    uint8_t  tstrb;
    uint32_t tid;
    uint32_t tdest;
    uint32_t tuser;
} HlsAxisSignals;

typedef struct {
    uint32_t max_burst_len;
    uint32_t burst_size;
    uint32_t cache_bits;
    uint32_t prot_bits;
    uint32_t qos;
    bool     use_read_alloc;
    bool     use_write_resp;
    uint32_t num_read_channels;
    uint32_t num_write_channels;
} HlsAxiMasterConfig;

typedef struct {
    char             name[64];
    uint32_t         port_count;
    char           **ports;
    HlsInterfaceType type;
} HlsBundle;

typedef struct {
    char             port_name[128];
    HlsInterfaceType type;
    HlsBundle       *bundle;
    uint32_t         depth;
    uint32_t         offset;
    bool             register_slice;
    bool             latency_unknown;
    uint32_t         max_latency;
} HlsInterfacePragma;

typedef struct {
    bool     enabled;
    uint32_t ii;
    bool     enable_flush;
    bool     rewind;
    char     target[128];
} HlsPipelineDirective;

typedef struct {
    bool     enabled;
    uint32_t factor;
    bool     skip_exit_check;
    char     region[128];
} HlsUnrollDirective;

typedef struct {
    bool             enabled;
    HlsArrayPartType type;
    uint32_t         dim;
    uint32_t         factor;
    bool             complete;
    char             variable[128];
} HlsArrayPartDirective;

typedef struct {
    bool     enabled;
    uint32_t max_tasks;
    bool     disable_start_propagation;
    char     region[128];
} HlsDataflowDirective;

typedef struct {
    char     operation[64];
    char     core_type[64];
    int32_t  limit;
    bool     use_latency;
    uint32_t latency;
} HlsResourceDirective;

typedef struct {
    HlsInterfacePragma   *interfaces;
    uint32_t              num_interfaces;
    HlsPipelineDirective  pipeline;
    HlsUnrollDirective   *unrolls;
    uint32_t              num_unrolls;
    HlsArrayPartDirective *array_parts;
    uint32_t              num_array_parts;
    HlsDataflowDirective   dataflow;
    HlsResourceDirective  *resources;
    uint32_t              num_resources;
    char                  top_function[128];
    uint32_t              clock_period_ns;
    bool                  reset_active_low;
} HlsPragmaSet;

void hls_interface_set(HlsPragmaSet *ps, const char *port,
       HlsInterfaceType type);
void hls_interface_bundle(HlsPragmaSet *ps, const char *bundle_name,
       const char **ports, uint32_t count, HlsInterfaceType type);
void hls_interface_axi_master(HlsPragmaSet *ps, const char *port,
       const HlsAxiMasterConfig *cfg);
void hls_interface_axi_stream(HlsPragmaSet *ps, const char *port,
       uint32_t depth);
void hls_interface_s_axilite(HlsPragmaSet *ps, const char *port);
void hls_interface_ap_ctrl_none(HlsPragmaSet *ps);

bool hls_axis_handshake(HlsAxisSignals *sig);
void hls_axis_valid_set(HlsAxisSignals *sig, bool v);
bool hls_axis_ready_get(const HlsAxisSignals *sig);
void hls_axis_signal_init(HlsAxisSignals *sig);
bool hls_axis_transfer_done(const HlsAxisSignals *sig);

void hls_axi_master_config_default(HlsAxiMasterConfig *cfg);
bool hls_axi_burst_read(HlsAxiMasterConfig *cfg, uint64_t addr,
       uint32_t len, void *buf);
bool hls_axi_burst_write(HlsAxiMasterConfig *cfg, uint64_t addr,
       uint32_t len, const void *buf);

HlsBundle* hls_bundle_create(const char *name, HlsInterfaceType type);
void       hls_bundle_destroy(HlsBundle *b);
bool       hls_bundle_add_port(HlsBundle *b, const char *port);

void hls_pragma_pipeline(HlsPragmaSet *ps, uint32_t ii, bool flush,
       bool rewind);
void hls_pragma_pipeline_set_target(HlsPragmaSet *ps, const char *target);
void hls_pragma_unroll(HlsPragmaSet *ps, uint32_t factor,
       const char *region);
void hls_pragma_array_partition(HlsPragmaSet *ps, const char *var,
       HlsArrayPartType type, uint32_t dim, uint32_t factor);
void hls_pragma_dataflow(HlsPragmaSet *ps, const char *region);
void hls_pragma_resource(HlsPragmaSet *ps, const char *op,
       const char *core, int32_t limit);

HlsPragmaSet* hls_pragma_set_create(void);
void          hls_pragma_set_destroy(HlsPragmaSet *ps);
void          hls_pragma_set_top(HlsPragmaSet *ps, const char *func_name);
void          hls_pragma_set_clock(HlsPragmaSet *ps, uint32_t period_ns);
bool          hls_pragma_validate(HlsPragmaSet *ps);
void          hls_pragma_print_tcl(HlsPragmaSet *ps, FILE *out);
void          hls_pragma_print_directives(HlsPragmaSet *ps, FILE *out);

#endif
