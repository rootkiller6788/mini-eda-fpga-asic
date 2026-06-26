#ifndef DATAFLOW_OPT_H
#define DATAFLOW_OPT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum {
    TASK_IDLE,
    TASK_RUNNING,
    TASK_STALLED,
    TASK_DONE
} HlsTaskState;

typedef enum {
    CHAN_FIFO,
    CHAN_PINGPONG,
    CHAN_STREAM,
    CHAN_SHARED_MEM
} HlsChannelType;

typedef struct {
    uint32_t depth;
    uint32_t width;
    bool     blocking_read;
    bool     blocking_write;
} HlsFifoConfig;

typedef struct {
    uint32_t buf_size;
    uint32_t num_buffers;
    bool     double_buf;
} HlsPingPongConfig;

typedef struct {
    uint32_t max_depth;
    uint32_t elem_width;
    bool     full;
    bool     empty;
    char     name[128];
} HlsStreamConfig;

typedef enum {
    STENCIL_1D,
    STENCIL_2D,
    STENCIL_3D
} HlsStencilDim;

typedef struct {
    HlsStencilDim dim;
    int32_t       radius[3];
    int32_t       window_size;
    uint32_t      line_buffer_depth;
    bool          use_shift_register;
} HlsStencilPattern;

typedef struct hls_channel {
    uint32_t       chan_id;
    HlsChannelType type;
    char           name[128];
    union {
        HlsFifoConfig     fifo;
        HlsPingPongConfig pingpong;
        HlsStreamConfig   stream;
    } config;
    uint32_t producer_count;
    uint32_t consumer_count;
    bool     is_pipelined;
    uint32_t transfer_latency;
} HlsChannel;

typedef struct hls_task {
    uint32_t          task_id;
    char              name[128];
    HlsTaskState      state;
    HlsChannel      **in_channels;
    uint32_t          num_in;
    HlsChannel      **out_channels;
    uint32_t          num_out;
    uint32_t          latency;
    uint32_t          interval;
    bool              is_pipelined;
    bool              has_stencil;
    HlsStencilPattern stencil;
    struct hls_task  *next;
    struct hls_task  *prev;
} HlsTask;

typedef struct {
    HlsTask    **tasks;
    uint32_t     num_tasks;
    HlsChannel **channels;
    uint32_t     num_channels;
    uint32_t     total_latency;
    bool         balanced;
    char         name[128];
} HlsDataflowGraph;

HlsDataflowGraph* hls_dataflow_create(const char *name);
void              hls_dataflow_destroy(HlsDataflowGraph *df);
HlsTask*          hls_dataflow_add_task(HlsDataflowGraph *df,
                    const char *name);
HlsChannel*       hls_dataflow_add_channel(HlsDataflowGraph *df,
                    HlsChannelType type, const char *name);
bool              hls_dataflow_connect(HlsTask *producer, HlsChannel *ch,
                    HlsTask *consumer);

bool hls_channel_fifo_configure(HlsChannel *ch, uint32_t depth,
       uint32_t width);
bool hls_channel_fifo_push(HlsChannel *ch, const void *data, uint32_t size);
bool hls_channel_fifo_pop(HlsChannel *ch, void *data, uint32_t size);
bool hls_channel_fifo_full(HlsChannel *ch);
bool hls_channel_fifo_empty(HlsChannel *ch);

bool  hls_channel_pingpong_configure(HlsChannel *ch, uint32_t buf_size);
bool  hls_channel_pingpong_swap(HlsChannel *ch);
void* hls_channel_pingpong_write_buf(HlsChannel *ch);
void* hls_channel_pingpong_read_buf(HlsChannel *ch);

bool hls_stream_create(HlsChannel *ch, uint32_t depth, uint32_t width);
bool hls_stream_write(HlsChannel *ch, const void *data);
bool hls_stream_read(HlsChannel *ch, void *data);
bool hls_stream_is_full(HlsChannel *ch);
bool hls_stream_is_empty(HlsChannel *ch);

bool hls_stencil_define(HlsTask *task, HlsStencilDim dim,
       const int32_t radius[3]);
bool hls_stencil_line_buffer(HlsTask *task, uint32_t rows);
bool hls_stencil_validate(HlsTask *task);

bool hls_dataflow_schedule(HlsDataflowGraph *df);
bool hls_dataflow_balance(HlsDataflowGraph *df);
bool hls_dataflow_verify(HlsDataflowGraph *df);
void hls_dataflow_report(HlsDataflowGraph *df, FILE *out);

void hls_task_start(HlsTask *task);
void hls_task_stall(HlsTask *task);
void hls_task_resume(HlsTask *task);
void hls_task_finish(HlsTask *task);
bool hls_task_is_stalled(HlsTask *task);

#endif
