#include "process.h"

static void cmd_ps(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vga_printf("%-4s %-12s %-10s %s\n", "PID", "NAME", "STATE", "TICKS");
    vga_puts("---- ------------ ---------- -----\n");
    for (int i = 0; i < MAX_PROCS; i++) {
        if (proc_table[i].state == PROC_UNUSED) continue;
        const char *states[] = {"UNUSED","READY","RUNNING","BLOCKED","ZOMBIE"};
        vga_printf("%-4d %-12s %-10s %u\n",
            proc_table[i].pid, proc_table[i].name,
            states[proc_table[i].state], proc_table[i].ticks);
    }
}
/* add to commands[] table: { "ps", "list processes", cmd_ps }, */