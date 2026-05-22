#ifndef WAVEFORM_VIEW_H
#define WAVEFORM_VIEW_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define WV_MAX_NAME_LEN        256
#define WV_MAX_SIGNALS         128
#define WV_MAX_SCOPES           32
#define WV_MAX_VALUE_CHANGES  8192
#define WV_MAX_DISPLAY_WIDTH   256
#define WV_MAX_DISPLAY_HEIGHT  128

typedef enum {
    WV_VAR_WIRE,
    WV_VAR_REG,
    WV_VAR_INTEGER,
    WV_VAR_REAL,
    WV_VAR_TIME,
    WV_VAR_STRING,
    WV_VAR_EVENT,
    WV_VAR_PARAMETER
} WvVarType;

typedef enum {
    WV_TIME_FS,
    WV_TIME_PS,
    WV_TIME_NS,
    WV_TIME_US,
    WV_TIME_MS,
    WV_TIME_S
} WvTimeUnit;

typedef enum {
    WV_TRANS_RISE,
    WV_TRANS_FALL,
    WV_TRANS_CHANGE,
    WV_TRANS_X,
    WV_TRANS_Z
} WvTransitionKind;

typedef struct {
    uint64_t            time;
    char               *value;
    int                 value_len;
} WvValueChange;

typedef struct {
    char                name[WV_MAX_NAME_LEN];
    char                identifier;
    int                 width;
    WvVarType           type;
    int                 scope_index;
    int                 change_count;
    WvValueChange       changes[WV_MAX_VALUE_CHANGES];
    bool                visible;
} WvSignal;

typedef struct {
    char                name[WV_MAX_NAME_LEN];
    char                type[WV_MAX_NAME_LEN];
    int                 parent_index;
    int                 signal_count;
    int                 signal_indices[WV_MAX_SIGNALS];
    int                 child_scope_count;
    int                 child_scope_indices[WV_MAX_SCOPES];
} WvScope;

typedef struct {
    uint64_t            time;
    WvTransitionKind    kind;
    int                 signal_index;
    char               *old_value;
    char               *new_value;
} WvTransition;

typedef struct {
    char                date[WV_MAX_NAME_LEN];
    char                version[WV_MAX_NAME_LEN];
    WvTimeUnit          timescale_unit;
    int                 timescale_magnitude;
    int                 scope_count;
    WvScope             scopes[WV_MAX_SCOPES];
    int                 root_scope_index;
    int                 signal_count;
    WvSignal            signals[WV_MAX_SIGNALS];
    int                 transition_count;
    WvTransition        transitions[WV_MAX_VALUE_CHANGES];
    uint64_t            start_time;
    uint64_t            end_time;
    FILE               *vcd_file;
} WvVcdData;

typedef struct {
    uint64_t            cursor_time;
    uint64_t            view_start;
    uint64_t            view_end;
    int                 selected_signal;
    int                 display_width;
    int                 display_height;
    char               *display_buffer;
    int                 buffer_size;
} WvViewer;

void        wv_init(WvVcdData *vcd);
void        wv_set_timescale(WvVcdData *vcd, int magnitude, WvTimeUnit unit);
bool        wv_parse_vcd(WvVcdData *vcd, const char *filename);
bool        wv_parse_vcd_header(WvVcdData *vcd);
bool        wv_parse_vcd_changes(WvVcdData *vcd);
int         wv_add_scope(WvVcdData *vcd, const char *name, const char *type, int parent);
int         wv_add_signal(WvVcdData *vcd, const char *name, char id, int width, WvVarType type, int scope);
void        wv_add_value_change(WvVcdData *vcd, int signal_idx, uint64_t time, const char *value);
WvSignal   *wv_find_signal_by_id(WvVcdData *vcd, char id);
WvSignal   *wv_find_signal_by_name(WvVcdData *vcd, const char *name);
void        wv_detect_transitions(WvVcdData *vcd);
void        wv_get_signal_value_at_time(const WvSignal *sig, uint64_t time, char *buf, int buf_size);
void        wv_get_signal_range(const WvSignal *sig, uint64_t start, uint64_t end,
                                WvValueChange **out_changes, int *out_count);
void        wv_init_viewer(WvViewer *viewer);
void        wv_set_view_range(WvViewer *viewer, uint64_t start, uint64_t end);
void        wv_render_ascii_waveform(const WvVcdData *vcd, const WvViewer *viewer, FILE *out);
void        wv_render_signal_trace(const WvSignal *sig, uint64_t start, uint64_t end,
                                   int display_width, char *buf, int buf_size);
void        wv_print_signal_list(const WvVcdData *vcd, FILE *out);
void        wv_print_time_info(const WvVcdData *vcd, uint64_t time, FILE *out);
const char *wv_time_unit_name(WvTimeUnit unit);
const char *wv_transition_kind_name(WvTransitionKind kind);
void        wv_free(WvVcdData *vcd);
void        wv_free_viewer(WvViewer *viewer);

#endif
