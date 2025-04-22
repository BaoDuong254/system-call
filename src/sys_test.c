#include "common.h"
#include "syscall.h"
#include "stdio.h"
#include "libmem.h"

int __sys_test(struct pcb_t *caller, struct sc_regs *regs)
{
    printf("sys_test: %s\n", caller->path);
    printf("sys_test: %d\n", caller->pid);
    return 0;
}