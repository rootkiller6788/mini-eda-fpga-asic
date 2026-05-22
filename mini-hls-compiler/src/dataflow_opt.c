#include "dataflow_opt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define HLS_MAX_TASKS    64
#define HLS_MAX_CHANNELS 64

HlsDataflowGraph* hls_dataflow_create(const char *name)
{
    HlsDataflowGraph *df = calloc(1, sizeof(HlsDataflowGraph));
    if (!df) return NULL;
    strncpy(df->name, name ? name : "dataflow", sizeof(df->name)-1);
    df->tasks = calloc(HLS_MAX_TASKS, sizeof(HlsTask*));
    df->channels = calloc(HLS_MAX_CHANNELS, sizeof(HlsChannel*));
    if (!df->tasks || !df->channels) {
        free(df->tasks); free(df->channels); free(df); return NULL;
    }
    return df;
}

void hls_dataflow_destroy(HlsDataflowGraph *df)
{
    if (!df) return;
    for (uint32_t i = 0; i < df->num_tasks; i++) {
        HlsTask *t = df->tasks[i];
        if (t) { free(t->in_channels); free(t->out_channels); free(t); }
    }
    for (uint32_t i = 0; i < df->num_channels; i++)
        free(df->channels[i]);
    free(df->tasks);
    free(df->channels);
    free(df);
}

HlsTask* hls_dataflow_add_task(HlsDataflowGraph *df, const char *name)
{
    if (!df || df->num_tasks >= HLS_MAX_TASKS) return NULL;
    HlsTask *t = calloc(1, sizeof(HlsTask));
    if (!t) return NULL;
    t->task_id = df->num_tasks;
    strncpy(t->name, name ? name : "", sizeof(t->name)-1);
    t->state = TASK_IDLE;
    t->in_channels = calloc(8, sizeof(HlsChannel*));
    t->out_channels = calloc(8, sizeof(HlsChannel*));
    if (!t->in_channels || !t->out_channels) {
        free(t->in_channels); free(t->out_channels); free(t); return NULL;
    }
    df->tasks[df->num_tasks++] = t;
    return t;
}

HlsChannel* hls_dataflow_add_channel(HlsDataflowGraph *df,
        HlsChannelType type, const char *name)
{
    if (!df || df->num_channels >= HLS_MAX_CHANNELS) return NULL;
    HlsChannel *ch = calloc(1, sizeof(HlsChannel));
    if (!ch) return NULL;
    ch->chan_id = df->num_channels;
    ch->type = type;
    strncpy(ch->name, name ? name : "", sizeof(ch->name)-1);
    df->channels[df->num_channels++] = ch;
    return ch;
}

bool hls_dataflow_connect(HlsTask *producer, HlsChannel *ch,
        HlsTask *consumer)
{
    if (!producer || !ch || !consumer) return false;
    if (producer->num_out < 8)
        producer->out_channels[producer->num_out++] = ch;
    if (consumer->num_in < 8)
        consumer->in_channels[consumer->num_in++] = ch;
    ch->producer_count++;
    ch->consumer_count++;
    return true;
}

bool hls_channel_fifo_configure(HlsChannel *ch, uint32_t depth,
        uint32_t width)
{
    if (!ch || depth == 0) return false;
    ch->type = CHAN_FIFO;
    ch->config.fifo.depth = depth;
    ch->config.fifo.width = width;
    ch->config.fifo.blocking_read = true;
    ch->config.fifo.blocking_write = true;
    return true;
}

bool hls_channel_fifo_push(HlsChannel *ch, const void *data, uint32_t size)
{
    (void)data;
    if (!ch || ch->type != CHAN_FIFO) return false;
    if (size >= ch->config.fifo.depth) return false;
    ch->config.fifo.blocking_write = false;
    return true;
}

bool hls_channel_fifo_pop(HlsChannel *ch, void *data, uint32_t size)
{
    (void)data;
    if (!ch || ch->type != CHAN_FIFO) return false;
    if (size >= ch->config.fifo.depth) return false;
    ch->config.fifo.blocking_read = false;
    return true;
}

bool hls_channel_fifo_full(HlsChannel *ch)
{
    if (!ch || ch->type != CHAN_FIFO) return false;
    return false;
}

bool hls_channel_fifo_empty(HlsChannel *ch)
{
    if (!ch || ch->type != CHAN_FIFO) return true;
    return true;
}

bool hls_channel_pingpong_configure(HlsChannel *ch, uint32_t buf_size)
{
    if (!ch || buf_size == 0) return false;
    ch->type = CHAN_PINGPONG;
    ch->config.pingpong.buf_size = buf_size;
    ch->config.pingpong.num_buffers = 2;
    ch->config.pingpong.double_buf = true;
    return true;
}

bool hls_channel_pingpong_swap(HlsChannel *ch)
{
    if (!ch || ch->type != CHAN_PINGPONG) return false;
    return true;
}

void* hls_channel_pingpong_write_buf(HlsChannel *ch)
{
    (void)ch;
    return NULL;
}

void* hls_channel_pingpong_read_buf(HlsChannel *ch)
{
    (void)ch;
    return NULL;
}

bool hls_stream_create(HlsChannel *ch, uint32_t depth, uint32_t width)
{
    if (!ch || depth == 0) return false;
    ch->type = CHAN_STREAM;
    ch->config.stream.max_depth = depth;
    ch->config.stream.elem_width = width;
    ch->config.stream.full = false;
    ch->config.stream.empty = true;
    return true;
}

bool hls_stream_write(HlsChannel *ch, const void *data)
{
    (void)data;
    if (!ch || ch->type != CHAN_STREAM) return false;
    if (ch->config.stream.full) return false;
    ch->config.stream.empty = false;
    return true;
}

bool hls_stream_read(HlsChannel *ch, void *data)
{
    (void)data;
    if (!ch || ch->type != CHAN_STREAM) return false;
    if (ch->config.stream.empty) return false;
    ch->config.stream.full = false;
    return true;
}

bool hls_stream_is_full(HlsChannel *ch)
{
    if (!ch || ch->type != CHAN_STREAM) return false;
    return ch->config.stream.full;
}

bool hls_stream_is_empty(HlsChannel *ch)
{
    if (!ch || ch->type != CHAN_STREAM) return true;
    return ch->config.stream.empty;
}

bool hls_stencil_define(HlsTask *task, HlsStencilDim dim,
        const int32_t radius[3])
{
    if (!task || !radius) return false;
    task->has_stencil = true;
    task->stencil.dim = dim;
    for (int i = 0; i < 3; i++)
        task->stencil.radius[i] = radius[i];
    int32_t ws = 1;
    for (int i = 0; i < (int)dim + 1; i++)
        ws *= (2 * radius[i] + 1);
    task->stencil.window_size = ws;
    task->stencil.use_shift_register = (ws <= 25);
    return true;
}

bool hls_stencil_line_buffer(HlsTask *task, uint32_t rows)
{
    if (!task || !task->has_stencil) return false;
    if (task->stencil.dim < STENCIL_2D) return false;
    task->stencil.line_buffer_depth = rows;
    return true;
}

bool hls_stencil_validate(HlsTask *task)
{
    if (!task || !task->has_stencil) return false;
    if (task->stencil.window_size <= 0) return false;
    if (task->stencil.dim >= STENCIL_2D &&
        task->stencil.line_buffer_depth == 0)
        return false;
    return true;
}

bool hls_dataflow_schedule(HlsDataflowGraph *df)
{
    if (!df) return false;
    HlsTask *prev = NULL;
    for (uint32_t i = 0; i < df->num_tasks; i++) {
        HlsTask *t = df->tasks[i];
        if (prev) { prev->next = t; t->prev = prev; }
        prev = t;
    }
    return true;
}

bool hls_dataflow_balance(HlsDataflowGraph *df)
{
    if (!df) return false;
    uint32_t max_lat = 0;
    for (uint32_t i = 0; i < df->num_tasks; i++) {
        HlsTask *t = df->tasks[i];
        if (t->latency > max_lat) max_lat = t->latency;
    }
    df->total_latency = max_lat;
    df->balanced = true;
    return true;
}

bool hls_dataflow_verify(HlsDataflowGraph *df)
{
    if (!df || df->num_tasks == 0) return false;
    for (uint32_t i = 0; i < df->num_channels; i++) {
        HlsChannel *ch = df->channels[i];
        if (ch->producer_count == 0 || ch->consumer_count == 0)
            return false;
    }
    return true;
}

void hls_dataflow_report(HlsDataflowGraph *df, FILE *out)
{
    if (!df || !out) return;
    fprintf(out, "=== Dataflow: %s ===\n", df->name);
    fprintf(out, "Tasks: %u, Channels: %u, Total latency: %u\n",
        df->num_tasks, df->num_channels, df->total_latency);
    for (uint32_t i = 0; i < df->num_tasks; i++) {
        HlsTask *t = df->tasks[i];
        fprintf(out, "  Task[%u] \"%s\": state=%d latency=%u "
            "pipelined=%d stencil=%d\n",
            t->task_id, t->name, t->state, t->latency,
            t->is_pipelined, t->has_stencil);
    }
    for (uint32_t i = 0; i < df->num_channels; i++) {
        HlsChannel *ch = df->channels[i];
        const char *tname =
            ch->type == CHAN_FIFO ? "FIFO" :
            ch->type == CHAN_PINGPONG ? "PingPong" :
            ch->type == CHAN_STREAM ? "Stream" : "SharedMem";
        fprintf(out, "  Channel[%u] \"%s\": type=%s prod=%u cons=%u\n",
            ch->chan_id, ch->name, tname,
            ch->producer_count, ch->consumer_count);
    }
}

void hls_task_start(HlsTask *task)
{
    if (task) task->state = TASK_RUNNING;
}

void hls_task_stall(HlsTask *task)
{
    if (task && task->state == TASK_RUNNING)
        task->state = TASK_STALLED;
}

void hls_task_resume(HlsTask *task)
{
    if (task && task->state == TASK_STALLED)
        task->state = TASK_RUNNING;
}

void hls_task_finish(HlsTask *task)
{
    if (task) task->state = TASK_DONE;
}

bool hls_task_is_stalled(HlsTask *task)
{
    return task ? task->state == TASK_STALLED : false;
}
