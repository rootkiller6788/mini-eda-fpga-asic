#include "testbench_gen.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void tb_init(TbTestbench *tb, const char *name, uint64_t end_time) {
    memset(tb, 0, sizeof(*tb));
    strncpy(tb->name, name, TB_MAX_NAME_LEN - 1);
    tb->end_time = end_time;
    tb->current_time = 0;
    tb->verbose = true;
}

int tb_add_clock(TbTestbench *tb, const char *name, int period, int duty_pct) {
    assert(tb->clock_count < 8);
    int idx = tb->clock_count++;
    TbClock *clk = &tb->clocks[idx];
    memset(clk, 0, sizeof(*clk));
    strncpy(clk->name, name, TB_MAX_NAME_LEN - 1);
    clk->period = period;
    clk->duty_cycle_pct = duty_pct > 0 ? duty_pct : 50;
    clk->mode = TB_CLK_FREE_RUNNING;
    clk->phase_offset = 0;
    clk->signal_index = idx;
    return idx;
}

void tb_set_clock_mode(TbClock *clk, TbClockMode mode) {
    clk->mode = mode;
}

int tb_add_reset(TbTestbench *tb, const char *name, int signal_idx, TbResetPolarity pol) {
    assert(tb->reset_count < 4);
    int idx = tb->reset_count++;
    TbReset *rst = &tb->resets[idx];
    memset(rst, 0, sizeof(*rst));
    strncpy(rst->name, name, TB_MAX_NAME_LEN - 1);
    rst->signal_index = signal_idx;
    rst->polarity = pol;
    rst->active_duration = 10;
    rst->deassert_time = 5;
    return idx;
}

void tb_set_reset_timing(TbReset *rst, int active_dur, int deassert_t) {
    rst->active_duration = active_dur;
    rst->deassert_time = deassert_t;
}

int tb_add_stimulus(TbTestbench *tb, const char *name, int signal_idx, TbStimulusType type) {
    assert(tb->stimulus_count < TB_MAX_STIMULI);
    int idx = tb->stimulus_count++;
    TbStimulus *stim = &tb->stimuli[idx];
    memset(stim, 0, sizeof(*stim));
    strncpy(stim->name, name, TB_MAX_NAME_LEN - 1);
    stim->signal_index = signal_idx;
    stim->type = type;
    stim->start_time = 0;
    stim->repeat_count = 1;
    stim->repeat_interval = 0;
    return idx;
}

void tb_stim_set(TbStimulus *stim, int value, int width) {
    stim->params.set.value = value;
    stim->params.set.width = width;
}

void tb_stim_pulse(TbStimulus *stim, int high_val, int low_val, int width, int high_dur, int low_dur) {
    stim->params.pulse.high_value = high_val;
    stim->params.pulse.low_value = low_val;
    stim->params.pulse.width = width;
    stim->params.pulse.high_duration = high_dur;
    stim->params.pulse.low_duration = low_dur;
}

void tb_stim_random(TbStimulus *stim, int min_val, int max_val, int width, int seed) {
    stim->params.random.min_val = min_val;
    stim->params.random.max_val = max_val;
    stim->params.random.width = width;
    stim->params.random.seed = seed;
}

void tb_stim_increment(TbStimulus *stim, int start_val, int incr) {
    stim->params.set.value = start_val;
    stim->params.set.width = incr;
}

void tb_set_repeat(TbStimulus *stim, int count, int interval) {
    stim->repeat_count = count;
    stim->repeat_interval = interval;
}

int tb_add_monitor(TbTestbench *tb, const char *name, int signal_idx, TbMonitorType type) {
    assert(tb->monitor_count < TB_MAX_MONITORS);
    int idx = tb->monitor_count++;
    TbMonitor *mon = &tb->monitors[idx];
    memset(mon, 0, sizeof(*mon));
    strncpy(mon->name, name, TB_MAX_NAME_LEN - 1);
    mon->signal_index = signal_idx;
    mon->type = type;
    mon->expected_value = 0;
    return idx;
}

void tb_monitor_set_expected(TbMonitor *mon, int expected) {
    mon->expected_value = expected;
}

void tb_monitor_set_display_fmt(TbMonitor *mon, const char *fmt) {
    strncpy(mon->display_format, fmt, TB_MAX_NAME_LEN - 1);
}

int tb_add_coverage_point(TbTestbench *tb, const char *name, int signal_idx) {
    assert(tb->coverage_count < TB_MAX_COVERAGE_POINTS);
    int idx = tb->coverage_count++;
    TbCoveragePoint *cp = &tb->coverage_points[idx];
    memset(cp, 0, sizeof(*cp));
    strncpy(cp->name, name, TB_MAX_NAME_LEN - 1);
    cp->signal_index = signal_idx;
    cp->type = TB_COV_BIN;
    return idx;
}

void tb_coverage_add_bin(TbCoveragePoint *cp, int low, int high) {
    assert(cp->bin_count < TB_MAX_COVERAGE_BINS);
    int idx = cp->bin_count++;
    cp->bins[idx].low = low;
    cp->bins[idx].high = high;
    cp->bins[idx].hit_count = 0;
}

void tb_evaluate_clock(TbTestbench *tb, TbClock *clk) {
    if (clk->mode != TB_CLK_FREE_RUNNING) return;
    uint64_t t = tb->current_time + (uint64_t)clk->phase_offset;
    uint64_t half_high = (uint64_t)(clk->period * clk->duty_cycle_pct / 100);
    uint64_t pos_in_period = t % (uint64_t)clk->period;
    int clk_val = (pos_in_period < half_high) ? 1 : 0;
    tb_record_waveform_sample(tb, clk->signal_index, clk_val);
}

void tb_evaluate_reset(TbTestbench *tb, TbReset *rst) {
    uint64_t t = tb->current_time;
    bool active;
    if (t < (uint64_t)rst->active_duration) {
        active = true;
    } else if (t < (uint64_t)(rst->active_duration + rst->deassert_time)) {
        active = false;
    } else {
        active = false;
    }
    int val = (rst->polarity == TB_RST_ACTIVE_HIGH) ? (active ? 1 : 0) : (active ? 0 : 1);
    tb_record_waveform_sample(tb, rst->signal_index, val);
}

void tb_evaluate_stimulus(TbTestbench *tb, TbStimulus *stim) {
    uint64_t t = tb->current_time;
    int value = 0;

    switch (stim->type) {
        case TB_STIM_SET:
            value = stim->params.set.value;
            break;
        case TB_STIM_PULSE: {
            uint64_t total_period = (uint64_t)(stim->params.pulse.high_duration +
                                                stim->params.pulse.low_duration);
            uint64_t pos = t % total_period;
            value = (pos < (uint64_t)stim->params.pulse.high_duration)
                    ? stim->params.pulse.high_value
                    : stim->params.pulse.low_value;
            break;
        }
        case TB_STIM_RANDOM: {
            int range = stim->params.random.max_val - stim->params.random.min_val + 1;
            value = stim->params.random.min_val + ((int)t * stim->params.random.seed) % range;
            break;
        }
        case TB_STIM_INCREMENT:
            value = stim->params.set.value + (int)t * stim->params.set.width;
            break;
        case TB_STIM_PATTERN:
            value = (int)t % stim->params.set.width;
            break;
    }

    tb_record_waveform_sample(tb, stim->signal_index, value);
}

bool tb_evaluate_monitor(TbTestbench *tb, TbMonitor *mon, int actual_value) {
    mon->check_count++;
    bool check_ok = true;

    switch (mon->type) {
        case TB_MON_ASSERT:
            check_ok = (actual_value == mon->expected_value);
            break;
        case TB_MON_DISPLAY:
            if (mon->display_format[0]) {
                printf(mon->display_format, mon->name, actual_value);
            } else {
                printf("[t=%llu] %s = %d\n", (unsigned long long)tb->current_time,
                       mon->name, actual_value);
            }
            break;
        case TB_MON_COUNT:
            check_ok = true;
            break;
        case TB_MON_TIMING:
            check_ok = true;
            break;
    }

    if (!check_ok) {
        mon->error_count++;
        tb->total_errors++;
    }
    tb->total_checks++;

    return check_ok;
}

void tb_update_coverage(TbTestbench *tb, TbCoveragePoint *cp, int value) {
    (void)tb;
    for (int b = 0; b < cp->bin_count; b++) {
        if (value >= cp->bins[b].low && value <= cp->bins[b].high) {
            cp->bins[b].hit_count++;
            cp->total_hits++;
        }
    }
}

void tb_record_waveform_sample(TbTestbench *tb, int signal_idx, int value) {
    TbWaveform *wf = &tb->waveform;
    if (signal_idx < 0 || signal_idx >= wf->signal_count) {
        if (signal_idx >= wf->signal_count) {
            wf->signal_count = signal_idx + 1;
        }
    }
    TbWaveformSignal *ws = &wf->signals[signal_idx];
    if (ws->sample_count < TB_MAX_TRANSACTIONS) {
        int idx = ws->sample_count++;
        ws->samples[idx].time = (int)tb->current_time;
        ws->samples[idx].signal_index = signal_idx;
        ws->samples[idx].value = value;
    }
}

void tb_run(TbTestbench *tb) {
    for (uint64_t t = 0; t < tb->end_time; t++) {
        tb->current_time = t;

        for (int c = 0; c < tb->clock_count; c++) {
            tb_evaluate_clock(tb, &tb->clocks[c]);
        }
        for (int r = 0; r < tb->reset_count; r++) {
            tb_evaluate_reset(tb, &tb->resets[r]);
        }
        for (int s = 0; s < tb->stimulus_count; s++) {
            tb_evaluate_stimulus(tb, &tb->stimuli[s]);
        }
    }
}

void tb_report(TbTestbench *tb) {
    printf("\n=== Testbench Report: %s ===\n", tb->name);
    printf("Total checks:  %d\n", tb->total_checks);
    printf("Total errors:  %d\n", tb->total_errors);
    printf("Status:        %s\n", tb->total_errors == 0 ? "PASS" : "FAIL");
    printf("\nMonitors:\n");
    for (int m = 0; m < tb->monitor_count; m++) {
        TbMonitor *mon = &tb->monitors[m];
        printf("  %s: %d checks, %d errors\n", mon->name, mon->check_count, mon->error_count);
    }
    tb_print_coverage(tb);
}

void tb_dump_waveform_vcd(TbTestbench *tb, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return;

    fprintf(f, "$date\n  Today\n$end\n");
    fprintf(f, "$version\n  mini-hdl-lang testbench\n$end\n");
    fprintf(f, "$timescale 1ns $end\n");
    fprintf(f, "$scope module %s $end\n", tb->name);

    TbWaveform *wf = &tb->waveform;
    for (int s = 0; s < wf->signal_count; s++) {
        fprintf(f, "$var wire 1 %c signal_%d $end\n", 'a' + s, s);
    }
    fprintf(f, "$upscope $end\n");
    fprintf(f, "$enddefinitions $end\n");

    for (uint64_t t = 0; t < tb->end_time; t++) {
        fprintf(f, "#%llu\n", (unsigned long long)t);
        for (int s = 0; s < wf->signal_count; s++) {
            TbWaveformSignal *ws = &wf->signals[s];
            int val = 0;
            for (int i = ws->sample_count - 1; i >= 0; i--) {
                if ((uint64_t)ws->samples[i].time <= t) {
                    val = ws->samples[i].value;
                    break;
                }
            }
            fprintf(f, "%d%c\n", val ? 1 : 0, 'a' + s);
        }
    }

    fclose(f);
    if (tb->verbose) printf("Waveform dumped to %s\n", filename);
}

void tb_print_coverage(TbTestbench *tb) {
    if (tb->coverage_count == 0) return;
    printf("\nCoverage:\n");
    for (int c = 0; c < tb->coverage_count; c++) {
        TbCoveragePoint *cp = &tb->coverage_points[c];
        int hit_bins = 0;
        for (int b = 0; b < cp->bin_count; b++) {
            if (cp->bins[b].hit_count > 0) hit_bins++;
        }
        float pct = cp->bin_count > 0 ? (float)hit_bins / (float)cp->bin_count * 100.0f : 0.0f;
        printf("  %s: %.1f%% (%d/%d bins hit)\n", cp->name, (double)pct, hit_bins, cp->bin_count);
    }
}

void tb_free(TbTestbench *tb) {
    memset(tb, 0, sizeof(*tb));
}
