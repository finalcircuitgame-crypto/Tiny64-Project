#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Simple syscall numbers
#define SYS_WRITE 1

void syscall_init();
int64_t sys_write(uint64_t fd, const char *buf, uint64_t len);

#ifdef __cplusplus
}
#endif

#endif // SYSCALL_H
