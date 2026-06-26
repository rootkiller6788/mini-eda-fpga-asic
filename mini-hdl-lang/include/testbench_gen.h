#ifndef TESTBENCH_GEN_H
#define TESTBENCH_GEN_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define TB_MAX_NAME_LEN        256
#define TB_MAX_SIGNALS         128
#define TB_MAX_STIMULI         256
#define TB_MAX_MONITORS         32
#define TB_MAX_COVERAGE_BINS    64
#define TB_MAX_COVERAGE_POINTS  32
#define TB_MAX_TRANSACTIONS    128
#define TB_MAX_WAVEFORM_SIGS    64

typedef enum {
    TB_CLK_FREE_RUNNING,
    TB_CLK_GATED,
    TB_CLK_DIVIDED
} TbClockMode;

typedef enum {
    TB_RST_ACTIVE_HIGH,
    TB_RST_ACTIVE_LOW
} TbResetPolarity;

typedef enum {
    TB_STIM_SET,
    TB_STIM_PULSE,
    TB_STIM_RANDOM,
    TB_STIM_INCREMENT,
    TB_STIM_PATTERN
} TbStimulusType;

typedef enum {
    TB_MON_ASSERT,
    TB_MON_DISPLAY,
    TB_MON_COUNT,
    TB_MON_TIMING
} TbMonitorType;

typedef enum {
    TB_COV_BIN,
    TB_COV_CROSS,
    TB_COV_TRANSITION
} TbCoverageType;

typedef struct {
    char                name[TB_MAX_NAME_LEN];
    int                 period;
    int                 duty_cycle_pct;
    int                 phase_offset;
    TbClockMode         mode;
    int                 signal_index;
} TbClock;

typedef struct {
    char                name[TB_MAX_NAME_LEN];
    int                 signal_index;
    TbResetPolarity     polarity;
    int                 active_duration;
    int                 deassert_time;
} TbReset;

typedef struct {
    char                name[TB_MAX_NAME_LEN];
    TbStimulusType      type;
    int                 signal_index;
    int                 start_time;
    int                 repeat_count;
    int                 repeat_interval;
    union {
        struct { int value; int width; } set;
        struct { int high_value; int low_value; int width; int high_duration; int low_duration; } pulse;
        struct { int min_val; int max_val; int width; int seed; } random;
    } params;
} TbStimulus;

typedef struct {
    char                name[TB_MAX_NAME_LEN];
    TbMonitorType       type;
    int                 signal_index;
    int                 expected_value;
    char                display_format[TB_MAX_NAME_LEN];
    bool                (*check_fn)(int actual, int expected);
    int                 error_count;
    int                 check_count;
} TbMonitor;

typedef struct {
    char                name[TB_MAX_NAME_LEN];
    TbCoverageType      type;
    int                 signal_index;
    int                 bin_count;
    struct {
        int low;
        int high;
        int hit_count;
    } bins[TB_MAX_COVERAGE_BINS];
    int                 total_hits;
} TbCoveragePoint;

typedef struct {
    int                 time;
    int                 signal_index;
    int                 value;
} TbWaveformSample;

typedef struct {
    char                name[TB_MAX_NAME_LEN];
    TbWaveformSample    samples[TB_MAX_TRANSACTIONS];
    int                 sample_count;
} TbWaveformSignal;

typedef struct {
    char                name[TB_MAX_NAME_LEN];
    int                 signal_count;
    TbWaveformSignal    signals[TB_MAX_WAVEFORM_SIGS];
} TbWaveform;

typedef struct {
    char                name[TB_MAX_NAME_LEN];
    int                 clock_count;
    TbClock             clocks[8];
    int                 reset_count;
    TbReset             resets[4];
    int                 stimulus_count;
    TbStimulus          stimuli[TB_MAX_STIMULI];
    int                 monitor_count;
    TbMonitor           monitors[TB_MAX_MONITORS];
    int                 coverage_count;
    TbCoveragePoint     coverage_points[TB_MAX_COVERAGE_POINTS];
    TbWaveform          waveform;
    uint64_t            current_time;
    uint64_t            end_time;
    int                 total_errors;
    int                 total_checks;
    bool                verbose;
} TbTestbench;

void        tb_init(TbTestbench *tb, const char *name, uint64_t end_time);
int         tb_add_clock(TbTestbench *tb, const char *name, int period, int duty_pct);
void        tb_set_clock_mode(TbClock *clk, TbClockMode mode);
int         tb_add_reset(TbTestbench *tb, const char *name, int signal_idx, TbResetPolarity pol);
void        tb_set_reset_timing(TbReset *rst, int active_dur, int deassert_t);
int         tb_add_stimulus(TbTestbench *tb, const char *name, int signal_idx, TbStimulusType type);
void        tb_stim_set(TbStimulus *stim, int value, int width);
void        tb_stim_pulse(TbStimulus *stim, int high_val, int low_val, int width, int high_dur, int low_dur);
void        tb_stim_random(TbStimulus *stim, int min_val, int max_val, int width, int seed);
void        tb_stim_increment(TbStimulus *stim, int start_val, int incr);
void        tb_set_repeat(TbStimulus *stim, int count, int interval);
int         tb_add_monitor(TbTestbench *tb, const char *name, int signal_idx, TbMonitorType type);
void        tb_monitor_set_expected(TbMonitor *mon, int expected);
void        tb_monitor_set_display_fmt(TbMonitor *mon, const char *fmt);
int         tb_add_coverage_point(TbTestbench *tb, const char *name, int signal_idx);
void        tb_coverage_add_bin(TbCoveragePoint *cp, int low, int high);
void        tb_evaluate_clock(TbTestbench *tb, TbClock *clk);
void        tb_evaluate_reset(TbTestbench *tb, TbReset *rst);
void        tb_evaluate_stimulus(TbTestbench *tb, TbStimulus *stim);
bool        tb_evaluate_monitor(TbTestbench *tb, TbMonitor *mon, int actual_value);
void        tb_update_coverage(TbTestbench *tb, TbCoveragePoint *cp, int value);
void        tb_record_waveform_sample(TbTestbench *tb, int signal_idx, int value);
void        tb_run(TbTestbench *tb);
void        tb_report(TbTestbench *tb);
void        tb_dump_waveform_vcd(TbTestbench *tb, const char *filename);
void        tb_print_coverage(TbTestbench *tb);
void        tb_free(TbTestbench *tb);

#endif
