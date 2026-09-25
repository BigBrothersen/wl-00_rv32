#ifndef __UART__
#define __UART__

// NS16550A-compatible UART, as found on QEMU virt (and most RISC-V SoCs and
// FPGA soft-SoCs). Registers are one byte wide, one byte apart.
#define UART_0 0x10000000

#define UART_RBR 0x00   // Receive buffer (read, DLAB=0)
#define UART_THR 0x00   // Transmit holding (write, DLAB=0)
#define UART_DLL 0x00   // Divisor latch low (DLAB=1)
#define UART_IER 0x01   // Interrupt enable (DLAB=0)
#define UART_DLM 0x01   // Divisor latch high (DLAB=1)
#define UART_FCR 0x02   // FIFO control (write)
#define UART_LCR 0x03   // Line control
#define UART_LSR 0x05   // Line status

#define UART_IER_RX      (1 << 0)   // Receive data available interrupt
#define UART_FCR_ENABLE  (1 << 0)
#define UART_FCR_CLEAR   (3 << 1)   // Clear RX and TX FIFOs
#define UART_LCR_8N1     0x03
#define UART_LCR_DLAB    (1 << 7)
#define UART_LSR_RX_READY (1 << 0)  // A byte is waiting in RBR
#define UART_LSR_TX_IDLE  (1 << 5)  // THR is empty, ready for next byte

#define UART_CLOCK_HZ 3686400       // QEMU virt; change per board when porting
#define UART_BAUD     115200

void uart_init();
void uart_putc(char c);
int uart_getc();

#endif
