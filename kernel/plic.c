#include <stdint.h>
#include "plic.h"
#include "proc.h"   // cpu_id()

// Set interrupt source priority. Runs once in main()
void init_plic() {
    *(uint32_t *)(PLIC + UART_0_IRQ * 4) = 1;
}

// Configure hart s-mode context. Every hart calls its own
void init_plichart() {
    int hart = cpu_id();
    *(uint32_t *)PLIC_SENABLE(hart) = (1 << UART_0_IRQ);
    *(uint32_t *)PLIC_STHRESH(hart) = 0;
}

// Highest-priority pending interrupt source in this hart s-mode context, 0 if none.
int plic_claim() {
    int hart = cpu_id();
    return *(uint32_t *)PLIC_SCLAIM(hart);
}

// mark service as complete; the PLIC may re-fire this source
void plic_complete(int irq) {
    int hart = cpu_id();
    *(uint32_t *)PLIC_SCLAIM(hart) = irq;
}