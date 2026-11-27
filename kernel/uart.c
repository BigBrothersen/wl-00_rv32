#include <stdint.h>
#include "uart.h"
#include "spinlock.h"
#include "kprint.h"
#include "proc.h"
#include "console.h"
#define UART_REG(r) ((volatile uint8_t *)(UART_0 + (r)))
#define UART_READ(reg) (*(UART_REG(reg)))
#define UART_WRITE(reg, v) (*(UART_REG(reg)) = (v))

static struct spinlock tx_lock;
uint32_t tx_chan;
uint32_t tx_busy;

// void uart_init()
// {
//     // set baud rate (assume 115200 baudrate)
//     // divisor = freq/baud rate
//     *(volatile uint32_t *)(UART_0 + UART_DIV) = 434;
//     *(volatile uint32_t *)(UART_0 + UART_IE) = 0x0; // disable interrupt
//     return;
// }

// Write buf to UART with size n
// void uart_write(char buf[], int n) 
// {
//     lock(&tx_lock);
//     int i = 0;
//     while (i < n) {
//         if ()
//     }
// }

void uart_init()
{
    *UART_REG(UART_IER) = 0x00; // disable interrupts while configuring

    // set baud rate: divisor = clock / (16 * baud)
    uint32_t divisor = UART_CLOCK_HZ / (16 * UART_BAUD);
    *UART_REG(UART_LCR) = UART_LCR_DLAB;
    *UART_REG(UART_DLL) = divisor & 0xff;
    *UART_REG(UART_DLM) = (divisor >> 8) & 0xff;

    *UART_REG(UART_LCR) = UART_LCR_8N1; // 8 data bits, no parity, 1 stop bit, DLAB off
    *UART_REG(UART_FCR) = UART_FCR_ENABLE | UART_FCR_CLEAR; // enable FIFO
    *UART_REG(UART_IER) = UART_IER_RX; // Enable RX data

    init_lock(&tx_lock, "uart");
}

// Called when there is a UART interrupt
void uart_intr()
{
    int c;
    while ((c = uart_getc()) != -1) console_intr(c);
}

// Blocking transmit: wait until the transmitter can take another byte.
void uart_putc(char c)
{
    while ((*UART_REG(UART_LSR) & UART_LSR_TX_IDLE) == 0)
        ;
    *UART_REG(UART_THR) = c;
    // tx_busy = 1;
}

// Non-blocking receive: returns the next byte, or -1 if none is waiting.
int uart_getc()
{
    if (*UART_REG(UART_LSR) & UART_LSR_RX_READY)
        return *UART_REG(UART_RBR);
    return -1;
}

void uart_write(char buf[], int n) 
{
    lock(&tx_lock);

    int i = 0;
    while (i < n) {

        // while (tx_busy != 0) {
        //     sleep(&tx_chan, &tx_lock);
        // }
        uart_putc(buf[i]);
        i += 1;
        tx_busy = 1;
    }
    unlock(&tx_lock);
}