#include "uvm_like.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
void uvm_init(UvmEnv *env) { memset(env, 0, sizeof(*env)); env->phase = UVM_BUILD; env->verbose = false; }
UvmComp *uvm_create_component(UvmEnv *env, const char *name, const char *type, UvmComp *parent) { if (env->comp_count >= MAX_UVM_COMPONENTS) return NULL; UvmComp *c = (UvmComp*)calloc(1, sizeof(UvmComp)); strncpy(c->name, name, sizeof(c->name)-1); strncpy(c->type, type, sizeof(c->type)-1); c->parent = parent; if (parent && parent->child_count < 8) parent->children[parent->child_count++] = c; if (!env->root) env->root = c; env->components[env->comp_count++] = c; return c; }
static void run_phase_recursive(UvmEnv *env, UvmComp *c) { if (!c) return; switch (env->phase) { case UVM_BUILD: if (c->build) c->build(c); break; case UVM_CONNECT: if (c->connect) c->connect(c); break; case UVM_RUN: if (c->run) c->run(c); break; default: break; } c->current_phase = env->phase; for (int i = 0; i < c->child_count; i++) run_phase_recursive(env, c->children[i]); }
void uvm_run_phase(UvmEnv *env, UvmPhase phase) { env->phase = phase; run_phase_recursive(env, env->root); }
bool uvm_run_test(UvmEnv *env, const char *test_name) { printf("=== UVM Test: %s ===\n", test_name); uvm_run_phase(env, UVM_BUILD); uvm_run_phase(env, UVM_CONNECT); uvm_run_phase(env, UVM_RUN); uvm_run_phase(env, UVM_CHECK); uvm_report(env); return true; }
void uvm_report(UvmEnv *env) { printf("=== UVM Report ===\n  Components: %d\n", env->comp_count); for (int i = 0; i < env->comp_count; i++) printf("  %s (%s) parent=%s children=%d\n", env->components[i]->name, env->components[i]->type, env->components[i]->parent ? env->components[i]->parent->name : "ROOT", env->components[i]->child_count); }
