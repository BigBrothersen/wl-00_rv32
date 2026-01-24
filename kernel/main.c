// main.c
#include <stdint.h>
#include "kprint.h"
#include "defs.h"
#include "mem.h"
#include "vm.h"
#include "trap.h"
#include "spinlock.h"
#include "proc.h"
#include "csr.h"

volatile static int init = 0;

void main() {
    if (r_tp() == 0){
        printf("cpu %d\n", r_tp());

        printf("\n");
        printf("wl-00 kernel is booting");
        printf("\n");
        init_bitmap();
        init_kptable();
        init_kvmhart();
        init_trap();
        init_proctable();
        init_userproc();
        // *(int*)0 = 5;
        // printf("before trap\n");
        // asm volatile("ecall");
        // printf("after trap\n");
        printf("Main cpu %d initialized!\n", r_tp());
        init = 1;
    }
    else {
        while(!init) {};
        printf("cpu %d initialized\n", r_tp());
    }
    scheduler();
    // while(1); // Don't exit
}