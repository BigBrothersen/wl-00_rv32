#include <stdint.h>
#include "uart.h"

void uart_init()
{
    // set baud rate (assume 115200 baudrate)
    // divisor = freq/baud rate
    
    *(volatile uint32_t *)(UART_0 + UART_DIV) = 434;

    *(volatile uint32_t *)(UART_0 + UART_IE) = 0x0; // disable interrupt
    return;
}
