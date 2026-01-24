#ifndef __GPR__
#define __GPR__

#include <stdint.h>

// Read/write t0
uint32_t r_t0(void);
void w_t0(uint32_t x);

// Read/write a0
uint32_t r_a0(void);
void w_a0(uint32_t x);

// Read/write a1
uint32_t r_a1(void);
void w_a1(uint32_t x);

// Read/write a2
uint32_t r_a2(void);
void w_a2(uint32_t x);

// Read/write a3
uint32_t r_a3(void);
void w_a3(uint32_t x);

// Read/write a4
uint32_t r_a4(void);
void w_a4(uint32_t x);

// Read/write a5
uint32_t r_a5(void);
void w_a5(uint32_t x);

uint32_t r_a6(void);
void w_a6(uint32_t x);

uint32_t r_a7(void);
void w_a7(uint32_t x);

#endif // REG_H
