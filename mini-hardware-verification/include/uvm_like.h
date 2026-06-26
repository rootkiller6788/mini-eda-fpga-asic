#ifndef UVM_LIKE_H
#define UVM_LIKE_H
#include <stdbool.h>
#define MAX_UVM_COMPONENTS 32
#define MAX_UVM_PHASES 8
typedef enum { UVM_BUILD, UVM_CONNECT, UVM_RUN, UVM_CHECK, UVM_REPORT, UVM_FINAL } UvmPhase;
typedef struct UvmComp UvmComp;
struct UvmComp { char name[64]; char type[32]; UvmComp *parent; UvmComp *children[8]; int child_count; UvmPhase current_phase; void (*build)(UvmComp *c); void (*connect)(UvmComp *c); void (*run)(UvmComp *c); };
typedef struct { UvmComp *root; UvmComp *components[MAX_UVM_COMPONENTS]; int comp_count; UvmPhase phase; bool verbose; } UvmEnv;
void uvm_init(UvmEnv *env);
UvmComp *uvm_create_component(UvmEnv *env, const char *name, const char *type, UvmComp *parent);
bool uvm_run_test(UvmEnv *env, const char *test_name);
void uvm_run_phase(UvmEnv *env, UvmPhase phase);
void uvm_report(UvmEnv *env);
#endif
