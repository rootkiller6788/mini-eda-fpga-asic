#ifndef ASSERTION_H
#define ASSERTION_H
#include <stdbool.h>
#define MAX_SEQUENCE_LEN 32
#define MAX_PROPERTIES 64
typedef enum { ASSERT_IMMEDIATE, ASSERT_CONCURRENT, ASSERT_SEQUENCE } AssertType;
typedef struct { bool values[MAX_SEQUENCE_LEN]; int len; } AssertSequence;
typedef struct { char name[64]; AssertType type; bool (*check)(void); int delay_min, delay_max; bool is_active; int failures; int passes; } AssertProperty;
typedef struct { AssertProperty props[MAX_PROPERTIES]; int prop_count; bool global_enable; } AssertEngine;
void assert_init(AssertEngine *e);
int assert_add_property(AssertEngine *e, const char *name, AssertType type, bool (*check)(void));
bool assert_check(AssertEngine *e, const char *name);
void assert_set_enable(AssertEngine *e, bool enable);
bool assert_sequence(AssertEngine *e, AssertSequence *seq, bool *result);
bool assert_until(AssertEngine *e, bool (*cond)(void), bool (*until)(void), int max_cycles);
void assert_report(AssertEngine *e);
#endif
