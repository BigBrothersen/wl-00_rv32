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
#include "uart.h"

// Set by hart 0 once the kernel is initialized. Lives in .data rather than
// .bss so it is valid before hart 0 has zeroed .bss (see boot_asm.S).
volatile static int init __attribute__((section(".data"))) = 0;

void main() {
    if (r_tp() == 0){
        uart_init();
        cpus[0].id = 0;
        printf("\n");
        printf("wl-00 kernel is booting\n");
        init_bitmap();
        init_kptable();
        init_kvmhart();
        init_trap();
        init_proctable();
        init_userproc();
        printf("Main cpu %d initialized!\n", r_tp());
        __sync_synchronize(); // publish everything above before releasing the other harts
        init = 1;
    }
    else {
        while(!init) {};
        __sync_synchronize();
        cpus[r_tp()].id = r_tp();
        init_kvmhart(); // turn on paging on this hart
        init_trap();    // install this hart's kernel trap vector
        printf("cpu %d initialized\n", r_tp());
    }
    scheduler();
}
