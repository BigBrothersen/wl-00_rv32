#ifndef __UART__
#define __UART__

#define UART_0 0x10000000
#define UART_TXFIFO 0x00
#define UART_RXINFO 0X04
#define UART_IE 0X08
#define UART_IP 0X0C
#define UART_DIV 0X18
// #define UART_STATE 0X1C
// #define UART_CTRL 0x20

void uart_init();


#endif