#include "interface_pragma.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

HlsPragmaSet* hls_pragma_set_create(void)
{
    HlsPragmaSet *ps = calloc(1, sizeof(HlsPragmaSet));
    if (!ps) return NULL;
    ps->interfaces = calloc(32, sizeof(HlsInterfacePragma));
    ps->unrolls = calloc(16, sizeof(HlsUnrollDirective));
    ps->array_parts = calloc(16, sizeof(HlsArrayPartDirective));
    ps->resources = calloc(16, sizeof(HlsResourceDirective));
    if (!ps->interfaces || !ps->unrolls || !ps->array_parts ||
        !ps->resources) {
        free(ps->interfaces); free(ps->unrolls);
        free(ps->array_parts); free(ps->resources); free(ps);
        return NULL;
    }
    ps->clock_period_ns = 10;
    ps->reset_active_low = true;
    return ps;
}

void hls_pragma_set_destroy(HlsPragmaSet *ps)
{
    if (!ps) return;
    for (uint32_t i = 0; i < ps->num_interfaces; i++) {
        HlsBundle *b = ps->interfaces[i].bundle;
        if (b) {
            for (uint32_t j = 0; j < b->port_count; j++)
                free(b->ports[j]);
            free(b->ports);
            free(b);
        }
    }
    free(ps->interfaces);
    free(ps->unrolls);
    free(ps->array_parts);
    free(ps->resources);
    free(ps);
}

void hls_pragma_set_top(HlsPragmaSet *ps, const char *func_name)
{
    if (ps && func_name)
        strncpy(ps->top_function, func_name,
            sizeof(ps->top_function)-1);
}

void hls_pragma_set_clock(HlsPragmaSet *ps, uint32_t period_ns)
{
    if (ps && period_ns > 0)
        ps->clock_period_ns = period_ns;
}

void hls_interface_set(HlsPragmaSet *ps, const char *port,
        HlsInterfaceType type)
{
    if (!ps || !port || ps->num_interfaces >= 32) return;
    HlsInterfacePragma *ifp = &ps->interfaces[ps->num_interfaces++];
    memset(ifp, 0, sizeof(*ifp));
    strncpy(ifp->port_name, port, sizeof(ifp->port_name)-1);
    ifp->type = type;
}

void hls_interface_bundle(HlsPragmaSet *ps, const char *bundle_name,
        const char **ports, uint32_t count, HlsInterfaceType type)
{
    if (!ps || !bundle_name || !ports || count == 0) return;
    HlsBundle *b = hls_bundle_create(bundle_name, type);
    if (!b) return;
    for (uint32_t i = 0; i < count; i++)
        hls_bundle_add_port(b, ports[i]);
    for (uint32_t i = 0; i < count; i++) {
        hls_interface_set(ps, ports[i], type);
        ps->interfaces[ps->num_interfaces - 1].bundle = b;
    }
}

void hls_interface_axi_master(HlsPragmaSet *ps, const char *port,
        const HlsAxiMasterConfig *cfg)
{
    if (!ps || !port) return;
    hls_interface_set(ps, port, IF_M_AXI);
    HlsInterfacePragma *ifp = &ps->interfaces[ps->num_interfaces - 1];
    if (cfg) {
        ifp->max_latency = cfg->max_burst_len;
        ifp->offset = 0;
        ifp->register_slice = true;
    }
}

void hls_interface_axi_stream(HlsPragmaSet *ps, const char *port,
        uint32_t depth)
{
    if (!ps || !port) return;
    hls_interface_set(ps, port, IF_AXIS);
    HlsInterfacePragma *ifp = &ps->interfaces[ps->num_interfaces - 1];
    ifp->depth = depth;
    ifp->register_slice = (depth > 0);
}

void hls_interface_s_axilite(HlsPragmaSet *ps, const char *port)
{
    if (ps && port)
        hls_interface_set(ps, port, IF_S_AXILITE);
}

void hls_interface_ap_ctrl_none(HlsPragmaSet *ps)
{
    if (ps)
        hls_interface_set(ps, "ap_ctrl", IF_AP_CTRL_NONE);
}

bool hls_axis_handshake(HlsAxisSignals *sig)
{
    return sig ? (sig->tvalid && sig->tready) : false;
}

void hls_axis_valid_set(HlsAxisSignals *sig, bool v)
{
    if (sig) sig->tvalid = v;
}

bool hls_axis_ready_get(const HlsAxisSignals *sig)
{
    return sig ? sig->tready : false;
}

void hls_axis_signal_init(HlsAxisSignals *sig)
{
    if (!sig) return;
    memset(sig, 0, sizeof(*sig));
    sig->tkeep = 0xFF;
    sig->tstrb = 0xFF;
}

bool hls_axis_transfer_done(const HlsAxisSignals *sig)
{
    return sig ? (sig->tvalid && sig->tready && sig->tlast) : false;
}

void hls_axi_master_config_default(HlsAxiMasterConfig *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->max_burst_len = 16;
    cfg->burst_size = 4;
    cfg->qos = 0;
    cfg->num_read_channels = 1;
    cfg->num_write_channels = 1;
}

bool hls_axi_burst_read(HlsAxiMasterConfig *cfg, uint64_t addr,
        uint32_t len, void *buf)
{
    (void)addr; (void)len; (void)buf;
    if (!cfg) return false;
    if (len > cfg->max_burst_len) return false;
    return true;
}

bool hls_axi_burst_write(HlsAxiMasterConfig *cfg, uint64_t addr,
        uint32_t len, const void *buf)
{
    (void)addr; (void)len; (void)buf;
    if (!cfg) return false;
    if (len > cfg->max_burst_len) return false;
    return true;
}

HlsBundle* hls_bundle_create(const char *name, HlsInterfaceType type)
{
    HlsBundle *b = calloc(1, sizeof(HlsBundle));
    if (!b) return NULL;
    strncpy(b->name, name, sizeof(b->name)-1);
    b->type = type;
    b->ports = calloc(16, sizeof(char*));
    if (!b->ports) { free(b); return NULL; }
    return b;
}

void hls_bundle_destroy(HlsBundle *b)
{
    if (!b) return;
    for (uint32_t i = 0; i < b->port_count; i++)
        free(b->ports[i]);
    free(b->ports);
    free(b);
}

bool hls_bundle_add_port(HlsBundle *b, const char *port)
{
    if (!b || !port || b->port_count >= 16) return false;
    b->ports[b->port_count] = malloc(strlen(port) + 1);
    if (!b->ports[b->port_count]) return false;
    strcpy(b->ports[b->port_count], port);
    b->port_count++;
    return true;
}

void hls_pragma_pipeline(HlsPragmaSet *ps, uint32_t ii, bool flush,
        bool rewind)
{
    if (!ps) return;
    ps->pipeline.enabled = true;
    ps->pipeline.ii = ii > 0 ? ii : 1;
    ps->pipeline.enable_flush = flush;
    ps->pipeline.rewind = rewind;
}

void hls_pragma_pipeline_set_target(HlsPragmaSet *ps,
        const char *target)
{
    if (ps && target)
        strncpy(ps->pipeline.target, target,
            sizeof(ps->pipeline.target)-1);
}

void hls_pragma_unroll(HlsPragmaSet *ps, uint32_t factor,
        const char *region)
{
    if (!ps || ps->num_unrolls >= 16) return;
    HlsUnrollDirective *u = &ps->unrolls[ps->num_unrolls++];
    memset(u, 0, sizeof(*u));
    u->enabled = true;
    u->factor = factor;
    if (region)
        strncpy(u->region, region, sizeof(u->region)-1);
}

void hls_pragma_array_partition(HlsPragmaSet *ps, const char *var,
        HlsArrayPartType type, uint32_t dim, uint32_t factor)
{
    if (!ps || !var || ps->num_array_parts >= 16) return;
    HlsArrayPartDirective *ap = &ps->array_parts[ps->num_array_parts++];
    memset(ap, 0, sizeof(*ap));
    ap->enabled = true;
    ap->type = type;
    ap->dim = dim;
    ap->factor = factor;
    ap->complete = (type == ARRAY_PART_COMPLETE);
    strncpy(ap->variable, var, sizeof(ap->variable)-1);
}

void hls_pragma_dataflow(HlsPragmaSet *ps, const char *region)
{
    if (!ps) return;
    ps->dataflow.enabled = true;
    if (region)
        strncpy(ps->dataflow.region, region,
            sizeof(ps->dataflow.region)-1);
}

void hls_pragma_resource(HlsPragmaSet *ps, const char *op,
        const char *core, int32_t limit)
{
    if (!ps || !op || !core || ps->num_resources >= 16) return;
    HlsResourceDirective *r = &ps->resources[ps->num_resources++];
    memset(r, 0, sizeof(*r));
    strncpy(r->operation, op, sizeof(r->operation)-1);
    strncpy(r->core_type, core, sizeof(r->core_type)-1);
    r->limit = limit;
}

bool hls_pragma_validate(HlsPragmaSet *ps)
{
    if (!ps) return false;
    if (ps->top_function[0] == '\0') return false;
    if (ps->clock_period_ns == 0) return false;
    if (ps->pipeline.enabled && ps->pipeline.ii == 0) return false;
    for (uint32_t i = 0; i < ps->num_array_parts; i++) {
        if (ps->array_parts[i].factor == 0 &&
            ps->array_parts[i].type != ARRAY_PART_COMPLETE)
            return false;
    }
    return true;
}

void hls_pragma_print_tcl(HlsPragmaSet *ps, FILE *out)
{
    if (!ps || !out) return;
    fprintf(out, "# mini-hls Tcl directives for %s\n",
        ps->top_function);
    fprintf(out, "open_project hls_project\n");
    fprintf(out, "set_top %s\n", ps->top_function);
    fprintf(out, "add_files source.c\n");
    fprintf(out, "open_solution \"solution1\"\n");
    fprintf(out, "set_part {xc7z020clg400-1}\n");
    fprintf(out, "create_clock -period %u\n\n", ps->clock_period_ns);
    for (uint32_t i = 0; i < ps->num_interfaces; i++) {
        HlsInterfacePragma *ifp = &ps->interfaces[i];
        const char *tname = "ap_none";
        switch (ifp->type) {
            case IF_M_AXI:    tname = "m_axi"; break;
            case IF_AXIS:     tname = "axis"; break;
            case IF_S_AXILITE: tname = "s_axilite"; break;
            case IF_AP_CTRL_NONE: tname = "ap_ctrl_none"; break;
            case IF_AP_FIFO:  tname = "ap_fifo"; break;
            default: break;
        }
        fprintf(out, "set_directive_interface -mode %s \"%s\" %s\n",
            tname, ps->top_function, ifp->port_name);
    }
    if (ps->pipeline.enabled)
        fprintf(out, "set_directive_pipeline -II %u \"%s\"\n",
            ps->pipeline.ii, ps->top_function);
    for (uint32_t i = 0; i < ps->num_unrolls; i++)
        fprintf(out, "set_directive_unroll -factor %u \"%s\"\n",
            ps->unrolls[i].factor,
            ps->unrolls[i].region[0] ? ps->unrolls[i].region
            : ps->top_function);
    for (uint32_t i = 0; i < ps->num_array_parts; i++) {
        const char *pt =
            ps->array_parts[i].type == ARRAY_PART_BLOCK ? "block" :
            ps->array_parts[i].type == ARRAY_PART_CYCLIC ? "cyclic" :
            "complete";
        fprintf(out,
            "set_directive_array_partition -type %s "
            "-factor %u -dim %u \"%s\" %s\n",
            pt, ps->array_parts[i].factor,
            ps->array_parts[i].dim, ps->top_function,
            ps->array_parts[i].variable);
    }
    if (ps->dataflow.enabled)
        fprintf(out, "set_directive_dataflow \"%s\"\n",
            ps->top_function);
    for (uint32_t i = 0; i < ps->num_resources; i++)
        fprintf(out,
            "set_directive_resource -core %s \"%s\" %s\n",
            ps->resources[i].core_type, ps->top_function,
            ps->resources[i].operation);
    fprintf(out, "\ncsynth_design\n");
    fprintf(out, "export_design -format ip_catalog\n");
}

void hls_pragma_print_directives(HlsPragmaSet *ps, FILE *out)
{
    if (!ps || !out) return;
    fprintf(out, "#pragma HLS interface summary for %s\n",
        ps->top_function);
    for (uint32_t i = 0; i < ps->num_interfaces; i++) {
        fprintf(out, "  interface port=%s type=%d\n",
            ps->interfaces[i].port_name,
            ps->interfaces[i].type);
    }
    fprintf(out, "  pipeline II=%u flush=%d rewind=%d\n",
        ps->pipeline.ii, ps->pipeline.enable_flush,
        ps->pipeline.rewind);
    fprintf(out, "  unroll directives: %u\n", ps->num_unrolls);
    fprintf(out, "  array_partition directives: %u\n",
        ps->num_array_parts);
    fprintf(out, "  dataflow: %d\n", ps->dataflow.enabled);
    fprintf(out, "  resource directives: %u\n", ps->num_resources);
}
