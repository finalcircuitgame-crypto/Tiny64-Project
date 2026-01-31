#ifndef TINY64_PANIC_H
#define TINY64_PANIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
} registers_t;

void panic(const char *msg, const char *file, int line);
void panic_with_regs(const char *msg, const char *file, int line, registers_t* regs);

#define PANIC(msg) panic(msg, __FILE__, __LINE__)

#ifdef __cplusplus
}
#endif

#endif
