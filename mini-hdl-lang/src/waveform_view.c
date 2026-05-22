#include "waveform_view.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

void wv_init(WvVcdData *vcd) {
    memset(vcd, 0, sizeof(*vcd));
    vcd->root_scope_index = -1;
    vcd->timescale_unit = WV_TIME_NS;
    vcd->timescale_magnitude = 1;
}

void wv_set_timescale(WvVcdData *vcd, int magnitude, WvTimeUnit unit) {
    vcd->timescale_magnitude = magnitude;
    vcd->timescale_unit = unit;
}

bool wv_parse_vcd(WvVcdData *vcd, const char *filename) {
    vcd->vcd_file = fopen(filename, "r");
    if (!vcd->vcd_file) return false;

    bool ok = wv_parse_vcd_header(vcd);
    if (ok) ok = wv_parse_vcd_changes(vcd);

    fclose(vcd->vcd_file);
    vcd->vcd_file = NULL;
    return ok;
}

bool wv_parse_vcd_header(WvVcdData *vcd) {
    if (!vcd->vcd_file) return false;

    FILE *f = vcd->vcd_file;
    char line[WV_MAX_NAME_LEN];
    int current_scope = -1;

    rewind(f);
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (isspace((unsigned char)*p)) p++;

        if (strncmp(p, "$date", 5) == 0) {
            if (fgets(line, sizeof(line), f)) {
                char *end = line;
                while (isspace((unsigned char)*end)) end++;
                size_t len = strlen(end);
                while (len > 0 && isspace((unsigned char)end[len - 1])) len--;
                memcpy(vcd->date, end, len < WV_MAX_NAME_LEN ? len : WV_MAX_NAME_LEN - 1);
            }
        } else if (strncmp(p, "$version", 8) == 0) {
            fgets(vcd->version, sizeof(vcd->version), f);
            p = vcd->version;
            while (isspace((unsigned char)*p)) p++;
            size_t len = strlen(p);
            while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r')) p[--len] = '\0';
        } else if (strncmp(p, "$timescale", 10) == 0) {
            fgets(line, sizeof(line), f);
            int mag = 1;
            sscanf(line, "%d", &mag);
            vcd->timescale_magnitude = mag > 0 ? mag : 1;
        } else if (strncmp(p, "$scope", 6) == 0) {
            char type[64] = "", name[WV_MAX_NAME_LEN] = "";
            sscanf(p, "$scope %63s %255s $end", type, name);
            current_scope = wv_add_scope(vcd, name, type,
                current_scope >= 0 ? current_scope : -1);
        } else if (strncmp(p, "$var", 4) == 0) {
            char var_type[16] = "", name[WV_MAX_NAME_LEN] = "";
            int width = 1;
            char id = 0;
            sscanf(p, "$var %15s %d %c %255s $end", var_type, &width, &id, name);

            WvVarType vt;
            if (strcmp(var_type, "wire") == 0) vt = WV_VAR_WIRE;
            else if (strcmp(var_type, "reg") == 0) vt = WV_VAR_REG;
            else if (strcmp(var_type, "integer") == 0) vt = WV_VAR_INTEGER;
            else if (strcmp(var_type, "real") == 0) vt = WV_VAR_REAL;
            else if (strcmp(var_type, "time") == 0) vt = WV_VAR_TIME;
            else vt = WV_VAR_WIRE;

            wv_add_signal(vcd, name, id, width, vt, current_scope >= 0 ? current_scope : 0);
        } else if (strncmp(p, "$upscope", 8) == 0) {
            current_scope = -1;
        } else if (strncmp(p, "$enddefinitions", 15) == 0) {
            break;
        }
    }
    return true;
}

bool wv_parse_vcd_changes(WvVcdData *vcd) {
    if (!vcd->vcd_file) return false;

    FILE *f = vcd->vcd_file;
    char line[WV_MAX_NAME_LEN];
    uint64_t current_time = 0;
    bool found_first = false;

    rewind(f);
    bool in_body = false;
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (!in_body) {
            if (strncmp(p, "$enddefinitions", 15) == 0) {
                in_body = true;
            }
            continue;
        }
        if (*p == '#') {
            current_time = strtoull(p + 1, NULL, 10);
            if (!found_first) {
                vcd->start_time = current_time;
                found_first = true;
            }
            vcd->end_time = current_time;
        } else if (*p == 'b' || *p == 'B') {
            char *val_start = p + 1;
            char *space = strchr(val_start, ' ');
            if (space) {
                char id = (char)space[1];
                *space = '\0';
                wv_add_value_change(vcd, -1, current_time, val_start);
                WvSignal *sig = wv_find_signal_by_id(vcd, id);
                if (sig) {
                    int ci = sig->change_count;
                    if (ci > 0) {
                        sig->changes[ci - 1].time = current_time;
                    }
                }
                *space = ' ';
            }
        } else if (isdigit((unsigned char)*p) || *p == 'x' || *p == 'X' ||
                   *p == 'z' || *p == 'Z') {
            char val[8] = {*p, '\0'};
            char id = 0;
            int idx = 1;
            while (p[idx] && !isspace((unsigned char)p[idx]) && p[idx] != '\n' && p[idx] != '\r') idx++;
            id = p[idx - 1];
            wv_add_value_change(vcd, -1, current_time, val);
        }
    }
    return true;
}

int wv_add_scope(WvVcdData *vcd, const char *name, const char *type, int parent) {
    assert(vcd->scope_count < WV_MAX_SCOPES);
    int idx = vcd->scope_count++;
    WvScope *scope = &vcd->scopes[idx];
    memset(scope, 0, sizeof(*scope));
    strncpy(scope->name, name, WV_MAX_NAME_LEN - 1);
    strncpy(scope->type, type, WV_MAX_NAME_LEN - 1);
    scope->parent_index = parent;

    if (vcd->root_scope_index < 0) vcd->root_scope_index = idx;
    return idx;
}

int wv_add_signal(WvVcdData *vcd, const char *name, char id, int width, WvVarType type, int scope) {
    assert(vcd->signal_count < WV_MAX_SIGNALS);
    int idx = vcd->signal_count++;
    WvSignal *sig = &vcd->signals[idx];
    memset(sig, 0, sizeof(*sig));
    strncpy(sig->name, name, WV_MAX_NAME_LEN - 1);
    sig->identifier = id;
    sig->width = width;
    sig->type = type;
    sig->scope_index = scope;
    sig->visible = true;
    return idx;
}

void wv_add_value_change(WvVcdData *vcd, int signal_idx, uint64_t time, const char *value) {
    if (signal_idx < 0) return;
    if (signal_idx >= vcd->signal_count) return;

    WvSignal *sig = &vcd->signals[signal_idx];
    if (sig->change_count >= WV_MAX_VALUE_CHANGES) return;

    int idx = sig->change_count++;
    sig->changes[idx].time = time;
    size_t len = strlen(value);
    sig->changes[idx].value = (char *)malloc(len + 1);
    if (sig->changes[idx].value) {
        memcpy(sig->changes[idx].value, value, len + 1);
    }
    sig->changes[idx].value_len = (int)len;
}

WvSignal *wv_find_signal_by_id(WvVcdData *vcd, char id) {
    for (int i = 0; i < vcd->signal_count; i++) {
        if (vcd->signals[i].identifier == id) return &vcd->signals[i];
    }
    return NULL;
}

WvSignal *wv_find_signal_by_name(WvVcdData *vcd, const char *name) {
    for (int i = 0; i < vcd->signal_count; i++) {
        if (strcmp(vcd->signals[i].name, name) == 0) return &vcd->signals[i];
    }
    return NULL;
}

void wv_detect_transitions(WvVcdData *vcd) {
    vcd->transition_count = 0;
    for (int s = 0; s < vcd->signal_count; s++) {
        WvSignal *sig = &vcd->signals[s];
        for (int c = 0; c < sig->change_count; c++) {
            if (vcd->transition_count >= WV_MAX_VALUE_CHANGES) break;
            WvTransition *tr = &vcd->transitions[vcd->transition_count++];
            tr->time = sig->changes[c].time;
            tr->signal_index = s;
            tr->old_value = NULL;
            tr->new_value = NULL;

            char cur_val = sig->changes[c].value ? sig->changes[c].value[0] : 'x';
            char prev_val = (c > 0 && sig->changes[c - 1].value) ? sig->changes[c - 1].value[0] : 'x';

            if (prev_val == '0' && cur_val == '1') tr->kind = WV_TRANS_RISE;
            else if (prev_val == '1' && cur_val == '0') tr->kind = WV_TRANS_FALL;
            else if (cur_val == 'x' || cur_val == 'X') tr->kind = WV_TRANS_X;
            else if (cur_val == 'z' || cur_val == 'Z') tr->kind = WV_TRANS_Z;
            else tr->kind = WV_TRANS_CHANGE;
        }
    }
}

void wv_get_signal_value_at_time(const WvSignal *sig, uint64_t time, char *buf, int buf_size) {
    if (!buf || buf_size < 2) return;
    buf[0] = 'x';
    buf[1] = '\0';
    for (int c = sig->change_count - 1; c >= 0; c--) {
        if (sig->changes[c].time <= time) {
            if (sig->changes[c].value) {
                strncpy(buf, sig->changes[c].value, (size_t)(buf_size - 1));
            }
            return;
        }
    }
}

void wv_get_signal_range(const WvSignal *sig, uint64_t start, uint64_t end,
                          WvValueChange **out_changes, int *out_count) {
    *out_changes = NULL;
    *out_count = 0;
    int count = 0;
    for (int c = 0; c < sig->change_count; c++) {
        if (sig->changes[c].time >= start && sig->changes[c].time <= end) count++;
    }
    if (count > 0) {
        *out_changes = &sig->changes[0];
        *out_count = count;
    }
}

void wv_init_viewer(WvViewer *viewer) {
    memset(viewer, 0, sizeof(*viewer));
    viewer->display_width = 80;
    viewer->display_height = 24;
    viewer->cursor_time = 0;
}

void wv_set_view_range(WvViewer *viewer, uint64_t start, uint64_t end) {
    viewer->view_start = start;
    viewer->view_end = end;
}

void wv_render_ascii_waveform(const WvVcdData *vcd, const WvViewer *viewer, FILE *out) {
    if (!out) out = stdout;

    fprintf(out, "=== ASCII Waveform Viewer ===\n");
    fprintf(out, "Time range: %llu - %llu\n",
            (unsigned long long)viewer->view_start,
            (unsigned long long)viewer->view_end);
    fprintf(out, "Timescale: %d %s\n\n", vcd->timescale_magnitude,
            wv_time_unit_name(vcd->timescale_unit));

    fprintf(out, "%-20s ", "Signal");
    for (int i = 0; i < viewer->display_width - 22; i++) fprintf(out, "-");
    fprintf(out, "\n");

    for (int s = 0; s < vcd->signal_count; s++) {
        const WvSignal *sig = &vcd->signals[s];
        if (!sig->visible) continue;

        fprintf(out, "%-20s ", sig->name);
        char trace[WV_MAX_DISPLAY_WIDTH + 1];
        wv_render_signal_trace(sig, viewer->view_start, viewer->view_end,
                                viewer->display_width - 22, trace, sizeof(trace));
        fprintf(out, "%s\n", trace);
    }

    fprintf(out, "%-20s ", "Time");
    uint64_t range = viewer->view_end - viewer->view_start;
    if (range == 0) range = 1;
    int disp_w = viewer->display_width - 22;
    for (int i = 0; i < disp_w; i++) {
        if (i == 0 || i == disp_w - 1) fprintf(out, "|");
        else fprintf(out, " ");
    }
    fprintf(out, "\n");
}

void wv_render_signal_trace(const WvSignal *sig, uint64_t start, uint64_t end,
                             int display_width, char *buf, int buf_size) {
    if (!buf || buf_size <= 1) return;
    memset(buf, ' ', (size_t)display_width);
    buf[display_width] = '\0';

    uint64_t range = end - start;
    if (range == 0) range = 1;

    char prev_val[64] = "x";
    int prev_col = 0;

    for (int x = 0; x < display_width; x++) {
        uint64_t t = start + (range * (uint64_t)x) / (uint64_t)display_width;
        char val[2] = {'x', '\0'};
        wv_get_signal_value_at_time(sig, t, val, sizeof(val));
        char ch = val[0];
        if (ch == '1') buf[x] = '-';
        else if (ch == '0') buf[x] = '_';
        else if (ch == 'x' || ch == 'X') buf[x] = 'X';
        else if (ch == 'z' || ch == 'Z') buf[x] = 'z';
        else buf[x] = '?';
    }
}

void wv_print_signal_list(const WvVcdData *vcd, FILE *out) {
    if (!out) out = stdout;
    fprintf(out, "Signals (%d total):\n", vcd->signal_count);
    for (int s = 0; s < vcd->signal_count; s++) {
        const WvSignal *sig = &vcd->signals[s];
        fprintf(out, "  [%c] %s (%d-bit, %d changes)\n",
                sig->identifier, sig->name, sig->width, sig->change_count);
    }
}

void wv_print_time_info(const WvVcdData *vcd, uint64_t time, FILE *out) {
    if (!out) out = stdout;
    fprintf(out, "Time: %llu %s\n", (unsigned long long)time,
            wv_time_unit_name(vcd->timescale_unit));
    fprintf(out, "Signal values:\n");
    for (int s = 0; s < vcd->signal_count; s++) {
        char val[64];
        wv_get_signal_value_at_time(&vcd->signals[s], time, val, sizeof(val));
        fprintf(out, "  %s = %s\n", vcd->signals[s].name, val);
    }
}

const char *wv_time_unit_name(WvTimeUnit unit) {
    static const char *names[] = {"fs", "ps", "ns", "us", "ms", "s"};
    if (unit <= WV_TIME_S) return names[unit];
    return "ns";
}

const char *wv_transition_kind_name(WvTransitionKind kind) {
    static const char *names[] = {"RISE", "FALL", "CHANGE", "X", "Z"};
    if (kind <= WV_TRANS_Z) return names[kind];
    return "?";
}

void wv_free(WvVcdData *vcd) {
    for (int s = 0; s < vcd->signal_count; s++) {
        WvSignal *sig = &vcd->signals[s];
        for (int c = 0; c < sig->change_count; c++) {
            free(sig->changes[c].value);
        }
    }
    for (int t = 0; t < vcd->transition_count; t++) {
        free(vcd->transitions[t].old_value);
        free(vcd->transitions[t].new_value);
    }
    memset(vcd, 0, sizeof(*vcd));
}

void wv_free_viewer(WvViewer *viewer) {
    free(viewer->display_buffer);
    memset(viewer, 0, sizeof(*viewer));
}
