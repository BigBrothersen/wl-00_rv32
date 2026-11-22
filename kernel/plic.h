#ifndef __PLIC__
#define __PLIC__

// 00001000 -- boot ROM, provided by qemu
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0
// 10001000 -- virtio disk (unused: no driver)
// 80000000 -- DRAM

#define PLIC 0x0C000000
#define UART_0_IRQ 10   // QEMU virt wires the UART to PLIC source 10

// S-mode context of hart h is PLIC context 2h+1 (contexts are interleaved
// M,S per hart). Priority word N is at PLIC + 4*N; source 0 is reserved.
#define PLIC_SENABLE(hart)  (PLIC + 0x2000   + 0x80   * (2 * (hart) + 1))
#define PLIC_STHRESH(hart)  (PLIC + 0x200000 + 0x1000 * (2 * (hart) + 1))
#define PLIC_SCLAIM(hart)   (PLIC + 0x200004 + 0x1000 * (2 * (hart) + 1))

void init_plic();
void init_plichart();
int  plic_claim();
void plic_complete(int irq);

#endif