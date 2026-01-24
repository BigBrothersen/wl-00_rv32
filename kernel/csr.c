#include <stdint.h>

// Machine Information Registers
uint32_t r_mvendorid() {
    uint32_t x;
    asm volatile("csrr %0, mvendorid" : "=r"(x));
    return x;
}

uint32_t r_marchid() {
    uint32_t x;
    asm volatile("csrr %0, marchid" : "=r"(x));
    return x;
}

uint32_t r_mimpid() {
    uint32_t x;
    asm volatile("csrr %0, mimpid" : "=r"(x));
    return x;
}

uint32_t r_mhartid() {
    uint32_t x;
    asm volatile("csrr %0, mhartid" : "=r"(x));
    return x;
}

// Machine Trap Setup
void w_mstatus(uint32_t x) {
    asm volatile("csrw mstatus, %0" : : "r"(x));
}

uint32_t r_mstatus() {
    uint32_t x;
    asm volatile("csrr %0, mstatus" : "=r"(x));
    return x;
}

void w_misa(uint32_t x) {
    asm volatile("csrw misa, %0" : : "r"(x));
}

uint32_t r_misa() {
    uint32_t x;
    asm volatile("csrr %0, misa" : "=r"(x));
    return x;
}

void w_medeleg(uint32_t x) {
    asm volatile("csrw medeleg, %0" : : "r"(x));
}

uint32_t r_medeleg() {
    uint32_t x;
    asm volatile("csrr %0, medeleg" : "=r"(x));
    return x;
}

void w_mideleg(uint32_t x) {
    asm volatile("csrw mideleg, %0" : : "r"(x));
}

uint32_t r_mideleg() {
    uint32_t x;
    asm volatile("csrr %0, mideleg" : "=r"(x));
    return x;
}

void w_mie(uint32_t x) {
    asm volatile("csrw mie, %0" : : "r"(x));
}

uint32_t r_mie() {
    uint32_t x;
    asm volatile("csrr %0, mie" : "=r"(x));
    return x;
}

void w_mtvec(uint32_t x) {
    x = x & ~0x3;
    asm volatile("csrw mtvec, %0" : : "r"(x));
}

uint32_t r_mtvec() {
    uint32_t x;
    asm volatile("csrr %0, mtvec" : "=r"(x));
    return x;
}

// Machine Trap Handling
uint32_t r_mscratch() {
    uint32_t x;
    asm volatile("csrr %0, mscratch" : "=r"(x));
    return x;
}

void w_mscratch(uint32_t x) {
    asm volatile("csrw mscratch, %0" : : "r"(x));
}

uint32_t r_mepc() {
    uint32_t x;
    asm volatile("csrr %0, mepc" : "=r"(x));
    return x;
}

void w_mepc(uint32_t x) {
    asm volatile("csrw mepc, %0" : : "r"(x));
}

uint32_t r_mcause() {
    uint32_t x;
    asm volatile("csrr %0, mcause" : "=r"(x));
    return x;
}

uint32_t r_mtval() {
    uint32_t x;
    asm volatile("csrr %0, mtval" : "=r"(x));
    return x;
}

uint32_t r_mip() {
    uint32_t x;
    asm volatile("csrr %0, mip" : "=r"(x));
    return x;
}

// Supervisor Trap Setup
void w_sstatus(uint32_t x) {
    asm volatile("csrw sstatus, %0" : : "r"(x));
}

uint32_t r_sstatus() {
    uint32_t x;
    asm volatile("csrr %0, sstatus" : "=r"(x));
    return x;
}

void w_sie(uint32_t x) {
    asm volatile("csrw sie, %0" : : "r"(x));
}

uint32_t r_sie() {
    uint32_t x;
    asm volatile("csrr %0, sie" : "=r"(x));
    return x;
}

void w_stvec(uint32_t x) {
    x = x & ~0x3;
    asm volatile("csrw stvec, %0" : : "r"(x));
}

uint32_t r_stvec() {
    uint32_t x;
    asm volatile("csrr %0, stvec" : "=r"(x));
    return x;
}

// Supervisor Trap Handling
uint32_t r_sscratch() {
    uint32_t x;
    asm volatile("csrr %0, sscratch" : "=r"(x));
    return x;
}

void w_sscratch(uint32_t x) {
    asm volatile("csrw sscratch, %0" : : "r"(x));
}

uint32_t r_sepc() {
    uint32_t x;
    asm volatile("csrr %0, sepc" : "=r"(x));
    return x;
}

void w_sepc(uint32_t x) {
    asm volatile("csrw sepc, %0" : : "r"(x));
}

uint32_t r_scause() {
    uint32_t x;
    asm volatile("csrr %0, scause" : "=r"(x));
    return x;
}

uint32_t r_stval() {
    uint32_t x;
    asm volatile("csrr %0, stval" : "=r"(x));
    return x;
}

uint32_t r_sip() {
    uint32_t x;
    asm volatile("csrr %0, sip" : "=r"(x));
    return x;
}

void w_sip(uint32_t x) {
    asm volatile("csrw sip, %0" : : "r"(x));
}

// Supervisor Protection and Translation
void w_satp(uint32_t x) {
    asm volatile("csrw satp, %0" : : "r"(x));
}

uint32_t r_satp() {
    uint32_t x;
    asm volatile("csrr %0, satp" : "=r"(x));
    return x;
}

// Machine Counters/Timers
uint32_t r_mcycle() {
    uint32_t x;
    asm volatile("csrr %0, mcycle" : "=r"(x));
    return x;
}

uint32_t r_minstret() {
    uint32_t x;
    asm volatile("csrr %0, minstret" : "=r"(x));
    return x;
}

// PMP Registers
void w_pmpcfg0(uint32_t x) {
    asm volatile("csrw pmpcfg0, %0" : : "r"(x));
}

uint32_t r_pmpcfg0() {
    uint32_t x;
    asm volatile("csrr %0, pmpcfg0" : "=r"(x));
    return x;
}

void w_pmpaddr0(uint32_t x) {
    asm volatile("csrw pmpaddr0, %0" : : "r"(x));
}

uint32_t r_pmpaddr0() {
    uint32_t x;
    asm volatile("csrr %0, pmpaddr0" : "=r"(x));
    return x;
}

void w_tp(uint32_t x) {
    asm volatile("mv tp, %0" : : "r" (x));
}

uint32_t r_tp() {
    uint32_t x;
    asm volatile("mv %0, tp" : "=r" (x));
    return x;
}

// 

// Add more PMP registers as needed (pmpcfg1-3, pmpaddr1-15)