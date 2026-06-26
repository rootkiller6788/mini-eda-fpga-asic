#ifndef UVM_METHODOLOGY_H
#define UVM_METHODOLOGY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ================================================================
   UVM Methodology — Testbench Hierarchy & Core Framework (C99)
   uvm_methodology.h
   ================================================================ */

/* ----- UVM Phases ----- */
typedef enum {
    UVM_PHASE_BUILD               = 0,
    UVM_PHASE_CONNECT             = 1,
    UVM_PHASE_END_OF_ELABORATION  = 2,
    UVM_PHASE_START_OF_SIMULATION = 3,
    UVM_PHASE_RUN                 = 4,
    UVM_PHASE_EXTRACT             = 5,
    UVM_PHASE_CHECK               = 6,
    UVM_PHASE_REPORT              = 7,
    UVM_PHASE_FINAL               = 8,
    UVM_PHASE_COUNT               = 9
} uvm_phase_t;

static inline const char* uvm_phase_name(uvm_phase_t ph) {
    static const char* names[] = {
        "build", "connect", "end_of_elaboration",
        "start_of_simulation", "run", "extract",
        "check", "report", "final"
    };
    return (ph < UVM_PHASE_COUNT) ? names[ph] : "unknown";
}

/* ----- Severity & Verbosity ----- */
typedef enum {
    UVM_NONE    = 0,
    UVM_LOW     = 100,
    UVM_MEDIUM  = 200,
    UVM_HIGH    = 300,
    UVM_FULL    = 400,
    UVM_DEBUG   = 500
} uvm_verbosity_t;

typedef enum {
    UVM_INFO    = 0,
    UVM_WARNING = 1,
    UVM_ERROR   = 2,
    UVM_FATAL   = 3
} uvm_severity_t;

/* ----- UVM Sequence Item ----- */
typedef struct uvm_sequence_item uvm_sequence_item_t;

typedef void (*uvm_seq_item_print_fn)(const uvm_sequence_item_t* item);
typedef void (*uvm_seq_item_copy_fn)(uvm_sequence_item_t* dst, const uvm_sequence_item_t* src);
typedef int  (*uvm_seq_item_compare_fn)(const uvm_sequence_item_t* a, const uvm_sequence_item_t* b);

struct uvm_sequence_item {
    uint32_t                seq_id;
    uint32_t                item_id;
    uint64_t                timestamp;
    char                    type_name[64];
    size_t                  data_size;
    uint8_t*                data;
    bool                    is_response;

    uvm_seq_item_print_fn   print;
    uvm_seq_item_copy_fn    copy;
    uvm_seq_item_compare_fn compare;
};

uvm_sequence_item_t* uvm_seq_item_create(const char* type_name, size_t data_size);
void uvm_seq_item_destroy(uvm_sequence_item_t* item);
void uvm_seq_item_set_data(uvm_sequence_item_t* item, const void* src, size_t sz);

/* ----- UVM Sequence ----- */
typedef struct uvm_sequence uvm_sequence_t;

typedef void (*uvm_seq_body_fn)(uvm_sequence_t* seq, uvm_sequencer_t* sqr);

struct uvm_sequence {
    char            name[64];
    uint32_t        seq_id;
    uvm_seq_body_fn body;
    void*           user_data;
    bool            is_running;
};

typedef struct uvm_sequencer uvm_sequencer_t;

/* ----- UVM Sequencer ----- */
#define UVM_SEQUENCER_MAX_SEQ 16
#define UVM_SEQUENCER_FIFO_DEPTH 256

struct uvm_sequencer {
    char              name[64];
    uvm_sequence_t*   active_sequences[UVM_SEQUENCER_MAX_SEQ];
    int               seq_count;
    uvm_sequence_item_t* fifo[UVM_SEQUENCER_FIFO_DEPTH];
    int               fifo_wr;
    int               fifo_rd;
    int               fifo_cnt;
    uint32_t          seed;
    void*             driver_ref;
};

uvm_sequencer_t* uvm_sequencer_create(const char* name);
void uvm_sequencer_destroy(uvm_sequencer_t* sqr);
bool uvm_sequencer_start_sequence(uvm_sequencer_t* sqr, uvm_sequence_t* seq);
void uvm_sequencer_stop_sequence(uvm_sequencer_t* sqr, uvm_sequence_t* seq);
bool uvm_sequencer_push_item(uvm_sequencer_t* sqr, uvm_sequence_item_t* item);
uvm_sequence_item_t* uvm_sequencer_pop_item(uvm_sequencer_t* sqr);
bool uvm_sequencer_has_item(uvm_sequencer_t* sqr);

/* ----- UVM Driver ----- */
typedef struct uvm_driver uvm_driver_t;

typedef void (*uvm_driver_run_fn)(uvm_driver_t* drv);
typedef void (*uvm_driver_reset_fn)(uvm_driver_t* drv);

struct uvm_driver {
    char               name[64];
    uvm_sequencer_t*   sequencer;
    uvm_driver_run_fn  run_phase;
    uvm_driver_reset_fn reset;
    bool               is_active;
    uint64_t           item_count;
    void*              user_data;
};

uvm_driver_t* uvm_driver_create(const char* name, uvm_sequencer_t* sqr);
void uvm_driver_destroy(uvm_driver_t* drv);
void uvm_driver_set_run_handler(uvm_driver_t* drv, uvm_driver_run_fn fn);
void uvm_driver_run(uvm_driver_t* drv);
void uvm_driver_reset(uvm_driver_t* drv);

/* ----- UVM Monitor ----- */
typedef struct uvm_monitor uvm_monitor_t;

typedef void (*uvm_monitor_sample_fn)(uvm_monitor_t* mon, const uvm_sequence_item_t* item);
typedef void (*uvm_monitor_run_fn)(uvm_monitor_t* mon);

struct uvm_monitor {
    char                  name[64];
    uvm_monitor_sample_fn sample;
    uvm_monitor_run_fn    run_phase;
    bool                  is_active;
    uint64_t              sample_count;
    void*                 analysis_port;  /* TLM analysis port */
    void*                 user_data;
};

uvm_monitor_t* uvm_monitor_create(const char* name);
void uvm_monitor_destroy(uvm_monitor_t* mon);
void uvm_monitor_set_sample_handler(uvm_monitor_t* mon, uvm_monitor_sample_fn fn);
void uvm_monitor_sample(uvm_monitor_t* mon, const uvm_sequence_item_t* item);

/* ----- UVM Scoreboard ----- */
typedef struct uvm_scoreboard uvm_scoreboard_t;

typedef bool (*uvm_sb_compare_fn)(uvm_scoreboard_t* sb,
    const uvm_sequence_item_t* predicted, const uvm_sequence_item_t* actual);

struct uvm_scoreboard {
    char             name[64];
    uvm_sb_compare_fn compare;
    uint64_t         match_count;
    uint64_t         mismatch_count;
    uint64_t         drop_count;
    void*            expected_fifo;
    void*            user_data;
};

uvm_scoreboard_t* uvm_scoreboard_create(const char* name, uvm_sb_compare_fn cmp);
void uvm_scoreboard_destroy(uvm_scoreboard_t* sb);
void uvm_scoreboard_add_expected(uvm_scoreboard_t* sb, const uvm_sequence_item_t* item);
bool uvm_scoreboard_check_actual(uvm_scoreboard_t* sb, const uvm_sequence_item_t* item);
void uvm_scoreboard_report(const uvm_scoreboard_t* sb);

/* ----- TLM Ports ----- */
typedef enum {
    TLM_PORT_PUT       = 0,
    TLM_PORT_GET       = 1,
    TLM_PORT_ANALYSIS  = 2,
    TLM_PORT_FIFO      = 3,
    TLM_PORT_BLOCKING  = 4
} tlm_port_type_t;

typedef struct tlm_port tlm_port_t;

typedef bool (*tlm_transport_fn)(tlm_port_t* port, uvm_sequence_item_t* item);
typedef bool (*tlm_nb_transport_fn)(tlm_port_t* port, uvm_sequence_item_t* item, uint64_t* delay);

struct tlm_port {
    char                name[64];
    tlm_port_type_t     type;
    tlm_transport_fn    transport;
    tlm_nb_transport_fn nb_transport;
    void*               peer;
    bool                is_connected;
    bool                is_export;
};

tlm_port_t* tlm_port_create(const char* name, tlm_port_type_t type);
void tlm_port_destroy(tlm_port_t* port);
bool tlm_port_connect(tlm_port_t* port, tlm_port_t* peer);
bool tlm_port_put(tlm_port_t* port, uvm_sequence_item_t* item);
bool tlm_port_get(tlm_port_t* port, uvm_sequence_item_t** item);
bool tlm_port_write_analysis(tlm_port_t* port, uvm_sequence_item_t* item);

/* ----- UVM Agent ----- */
typedef struct uvm_agent uvm_agent_t;

typedef enum {
    UVM_AGENT_ACTIVE  = 0,
    UVM_AGENT_PASSIVE = 1
} uvm_agent_type_t;

struct uvm_agent {
    char           name[64];
    uvm_agent_type_t agent_type;
    uvm_driver_t*  driver;
    uvm_monitor_t* monitor;
    uvm_sequencer_t* sequencer;
    tlm_port_t*    monitor_ap;  /* analysis port from monitor */
    bool           is_configured;
};

uvm_agent_t* uvm_agent_create(const char* name, uvm_agent_type_t type);
void uvm_agent_destroy(uvm_agent_t* agent);
void uvm_agent_connect(uvm_agent_t* agent);

/* ----- UVM Environment ----- */
typedef struct uvm_env uvm_env_t;

struct uvm_env {
    char        name[64];
    uvm_agent_t** agents;
    int         agent_count;
    int         agent_capacity;
    uvm_scoreboard_t* scoreboard;
    bool        is_built;
};

uvm_env_t* uvm_env_create(const char* name);
void uvm_env_destroy(uvm_env_t* env);
void uvm_env_add_agent(uvm_env_t* env, uvm_agent_t* agent);
void uvm_env_set_scoreboard(uvm_env_t* env, uvm_scoreboard_t* sb);

/* ----- UVM Test ----- */
typedef struct uvm_test uvm_test_t;

typedef void (*uvm_test_run_fn)(uvm_test_t* test);

struct uvm_test {
    char            name[64];
    uvm_env_t*      env;
    uvm_test_run_fn  run;
    uvm_verbosity_t verbosity;
    uint64_t        timeout_ns;
    uint32_t        seed;
    bool            pass;
    int             fail_count;
};

uvm_test_t* uvm_test_create(const char* name);
void uvm_test_destroy(uvm_test_t* test);
void uvm_test_set_env(uvm_test_t* test, uvm_env_t* env);
void uvm_test_set_run_handler(uvm_test_t* test, uvm_test_run_fn fn);
bool uvm_test_run(uvm_test_t* test);
bool uvm_test_report(const uvm_test_t* test);

/* ----- UVM Factory (Registration & Override) ----- */
typedef struct uvm_factory uvm_factory_t;
typedef struct uvm_component_registry uvm_component_reg_t;

typedef void* (*uvm_factory_create_fn)(const char* name);

struct uvm_component_registry {
    char                    type_name[64];
    uvm_factory_create_fn   create_fn;
    char                    override_type[64];
    uvm_factory_create_fn   override_fn;
    uvm_component_reg_t*    next;
};

struct uvm_factory {
    uvm_component_reg_t* registry;
    int registry_count;
};

uvm_factory_t* uvm_factory_get(void);
void uvm_factory_register(uvm_factory_t* f,
    const char* type_name, uvm_factory_create_fn create_fn);
void uvm_factory_set_override(uvm_factory_t* f,
    const char* original_type, const char* override_type,
    uvm_factory_create_fn create_fn);
void* uvm_factory_create_component(uvm_factory_t* f,
    const char* type_name, const char* instance_name);

/* ----- Message / Logging ----- */
void uvm_msg(uvm_severity_t severity, const char* id,
    const char* msg, uvm_verbosity_t verbosity, uvm_verbosity_t threshold);
void uvm_msg_info(const char* id, const char* fmt, ...);
void uvm_msg_warning(const char* id, const char* fmt, ...);
void uvm_msg_error(const char* id, const char* fmt, ...);
void uvm_msg_fatal(const char* id, const char* fmt, ...);

/* ----- Global Phase Controller ----- */
typedef struct uvm_phase_ctrl uvm_phase_ctrl_t;

struct uvm_phase_ctrl {
    uvm_phase_t current_phase;
    bool        phase_done[UVM_PHASE_COUNT];
    int         raised_count;
    int         dropped_count;
};

uvm_phase_ctrl_t* uvm_phase_ctrl_get(void);
void uvm_phase_ctrl_transition(uvm_phase_ctrl_t* ctrl, uvm_phase_t next);
void uvm_phase_raise_objection(uvm_phase_ctrl_t* ctrl);
void uvm_phase_drop_objection(uvm_phase_ctrl_t* ctrl);
bool uvm_phase_is_done(uvm_phase_ctrl_t* ctrl);

/* ----- Default comparison helpers ----- */
int uvm_default_compare(const uvm_sequence_item_t* a,
    const uvm_sequence_item_t* b);
void uvm_default_print(const uvm_sequence_item_t* item);
void uvm_default_copy(uvm_sequence_item_t* dst,
    const uvm_sequence_item_t* src);

#endif /* UVM_METHODOLOGY_H */
