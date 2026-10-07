#include <stdint.h>
#include "uart.h"

#define UART_REG(r) ((volatile uint8_t *)(UART_0 + (r)))

// void uart_init()
// {
//     // set baud rate (assume 115200 baudrate)
//     // divisor = freq/baud rate
//     *(volatile uint32_t *)(UART_0 + UART_DIV) = 434;
//     *(volatile uint32_t *)(UART_0 + UART_IE) = 0x0; // disable interrupt
//     return;
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
    *UART_REG(UART_FCR) = UART_FCR_ENABLE | UART_FCR_CLEAR;
}

// Blocking transmit: wait until the transmitter can take another byte.
void uart_putc(char c)
{
    while ((*UART_REG(UART_LSR) & UART_LSR_TX_IDLE) == 0)
        ;
    *UART_REG(UART_THR) = c;
}

// Non-blocking receive: returns the next byte, or -1 if none is waiting.
int uart_getc()
{
    if (*UART_REG(UART_LSR) & UART_LSR_RX_READY)
        return *UART_REG(UART_RBR);
    return -1;
}
