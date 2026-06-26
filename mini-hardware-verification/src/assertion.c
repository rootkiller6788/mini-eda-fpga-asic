#include "assertion.h"
#include <stdio.h>
#include <string.h>
void assert_init(AssertEngine *e) { memset(e, 0, sizeof(*e)); e->global_enable = true; }
int assert_add_property(AssertEngine *e, const char *name, AssertType type, bool (*check)(void)) { if (e->prop_count >= MAX_PROPERTIES) return -1; AssertProperty *p = &e->props[e->prop_count]; strncpy(p->name, name, sizeof(p->name)-1); p->type = type; p->check = check; p->is_active = true; p->delay_min = 0; p->delay_max = 100; return e->prop_count++; }
bool assert_check(AssertEngine *e, const char *name) { if (!e->global_enable) return true; for (int i = 0; i < e->prop_count; i++) { if (strcmp(e->props[i].name, name) == 0 && e->props[i].is_active) { if (e->props[i].check && e->props[i].check()) { e->props[i].passes++; return true; } else { e->props[i].failures++; printf("[ASSERT] %s FAILED\n", name); return false; } } } return false; }
void assert_set_enable(AssertEngine *e, bool enable) { e->global_enable = enable; }
bool assert_sequence(AssertEngine *e, AssertSequence *seq, bool *result) { (void)e; for (int i = 0; i < seq->len; i++) if (!seq->values[i]) { *result = false; return true; } *result = true; return true; }
bool assert_until(AssertEngine *e, bool (*cond)(void), bool (*until)(void), int max_cycles) { (void)e; for (int i = 0; i < max_cycles; i++) { if (until()) return true; if (!cond()) return false; } return false; }
void assert_report(AssertEngine *e) { printf("=== Assertion Report ===\n"); for (int i = 0; i < e->prop_count; i++) printf("  %s: %d passes, %d failures\n", e->props[i].name, e->props[i].passes, e->props[i].failures); }
