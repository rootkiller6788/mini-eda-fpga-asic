#ifndef TESTBENCH_H
#define TESTBENCH_H
#include <stdbool.h>
#define MAX_TB_SIGNALS 64
#define MAX_TB_TRANSACTIONS 256
#define MAX_TB_MONITORS 8
typedef enum { TB_RESET, TB_IDLE, TB_DRIVE, TB_MONITOR, TB_CHECK, TB_DONE } TbPhase;
typedef struct { int val, width; char name[32]; } TbSignal;
typedef struct { TbSignal signals[MAX_TB_SIGNALS]; int signal_count; TbPhase phase; int cycle; char name[64]; bool verbose; int errors; } Testbench;
void tb_init(Testbench *tb, const char *name);
int tb_add_signal(Testbench *tb, const char *name, int width);
void tb_drive(Testbench *tb, const char *sig, int value);
int tb_monitor(Testbench *tb, const char *sig);
bool tb_compare(Testbench *tb, const char *sig, int expected);
void tb_cycle(Testbench *tb);
void tb_run(Testbench *tb, int cycles);
void tb_report(Testbench *tb);
#endif
