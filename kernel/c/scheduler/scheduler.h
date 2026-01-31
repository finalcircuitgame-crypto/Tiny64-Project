#ifndef TINY64_SCHEDULER_H
#define TINY64_SCHEDULER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TASKS 32

typedef enum {
    TASK_RUNNABLE = 0,
    TASK_BLOCKED = 1,
    TASK_SLEEPING = 2,
    TASK_UNUSED = 3
} task_state_t;

typedef struct {
    uint64_t rsp;
    int id;
    char name[32];
    task_state_t state;
    int priority;
    uint32_t sleep_ticks;
    void* stack_base;
} task_t;

extern task_t g_tasks[MAX_TASKS];
int scheduler_get_current_idx();
extern uint64_t g_ticks;
uint64_t timer_get_ms();

void scheduler_init();
void scheduler_tick();
int create_task(void (*entry)(), int priority, const char* name);
void yield();
void task_sleep(uint32_t ticks);

#ifdef __cplusplus
}
#endif

#endif