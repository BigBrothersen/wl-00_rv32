// Contains module to read/write general purpose register
#include <stdint.h>

uint32_t r_t0(){
    uint32_t x;
    asm volatile("mv %0, a0" : "=r"(x));
}

void w_t0(uint32_t x) {
    asm volatile("mv t0, %0" : : "r"(x));
}


uint32_t r_a0(){
    uint32_t x;
    asm volatile("mv %0, a0" : "=r"(x));
    return x;
}

void w_a0(uint32_t x) {
    asm volatile("mv a0, %0" : : "r"(x));
}

uint32_t r_a1(){
    uint32_t x;
    asm volatile("mv %0, a1" : "=r"(x));
    return x;
}

void w_a1(uint32_t x) {
    asm volatile("mv a1, %0" : : "r"(x));
}

uint32_t r_a2(){
    uint32_t x;
    asm volatile("mv %0, a2" : "=r"(x));
    return x;
}

void w_a2(uint32_t x) {
    asm volatile("mv a2, %0" : : "r"(x));
}

uint32_t r_a3(){
    uint32_t x;
    asm volatile("mv %0, a3" : "=r"(x));
    return x;
}

void w_a3(uint32_t x) {
    asm volatile("mv a3, %0" : : "r"(x));
}

uint32_t r_a4(){
    uint32_t x;
    asm volatile("mv %0, a4" : "=r"(x));
    return x;
}

void w_a4(uint32_t x) {
    asm volatile("mv a4, %0" : : "r"(x));
}

uint32_t r_a5(){
    uint32_t x;
    asm volatile("mv %0, a5" : "=r"(x));
    return x;
}

void w_a5(uint32_t x) {
    asm volatile("mv a5, %0" : : "r"(x));
}

uint32_t r_a6(){
    uint32_t x;
    asm volatile("mv %0, a6" : "=r"(x));
    return x;
}

void w_a6(uint32_t x) {
    asm volatile("mv a6, %0" : : "r"(x));
}

uint32_t r_a7(){
    uint32_t x;
    asm volatile("mv %0, a7" : "=r"(x));
    return x;
}

void w_a7(uint32_t x) {
    asm volatile("mv a7, %0" : : "r"(x));
}