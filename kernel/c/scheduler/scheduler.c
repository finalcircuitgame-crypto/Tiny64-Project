#include "scheduler.h"
#include "../memory/heap.h"
#include "../lib/string.h"
#include "../panic/panic.h"
#include "../drivers/platform/serial.h"

extern void switch_context(uint64_t* old_rsp, uint64_t new_rsp);

task_t g_tasks[MAX_TASKS];
static int current_task_idx = 0;
uint64_t g_ticks = 0;

int scheduler_get_current_idx() {
    return current_task_idx;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

void scheduler_init() {
    // Initialize PIT to 1000Hz
    uint32_t divisor = 1193182 / 1000;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));

    memset(g_tasks, 0, sizeof(g_tasks));
    for (int i = 0; i < MAX_TASKS; i++) {
        g_tasks[i].state = TASK_UNUSED;
    }
    
    // Create the "Main" task for current execution
    g_tasks[0].id = 0;
    strcpy(g_tasks[0].name, "Kernel");
    g_tasks[0].state = TASK_RUNNABLE;
    g_tasks[0].priority = 0;
    current_task_idx = 0;
}

uint64_t timer_get_ms() {
    return g_ticks;
}

int create_task(void (*entry)(), int priority, const char* name) {
    for (int i = 1; i < MAX_TASKS; i++) {
        if (g_tasks[i].state == TASK_UNUSED) {
            void* stack = kmalloc(4096);
            memset(stack, 0, 4096);
            
            uint64_t* rsp = (uint64_t*)((uintptr_t)stack + 4096);
            
            // Setup initial stack frame for switch_context
            *(--rsp) = (uint64_t)entry; // Return address for ret in switch_context
            *(--rsp) = 0; // rbp
            *(--rsp) = 0; // rbx
            *(--rsp) = 0; // r12
            *(--rsp) = 0; // r13
            *(--rsp) = 0; // r14
            *(--rsp) = 0; // r15

            g_tasks[i].rsp = (uint64_t)rsp;
            g_tasks[i].stack_base = stack;
            g_tasks[i].state = TASK_RUNNABLE;
            g_tasks[i].priority = priority;
            g_tasks[i].id = i;
            if (name) strncpy(g_tasks[i].name, name, 31);
            else ksprintf(g_tasks[i].name, "Task %d", i);
            return i;
        }
    }
    return -1;
}

static int get_next_task() {
    int highest_priority = -1;
    int next_idx = -1;

    for (int i = 0; i < MAX_TASKS; i++) {
        int idx = (current_task_idx + i + 1) % MAX_TASKS;
        if (g_tasks[idx].state == TASK_RUNNABLE) {
            if (g_tasks[idx].priority > highest_priority) {
                highest_priority = g_tasks[idx].priority;
                next_idx = idx;
            }
        }
    }
    
    if (next_idx == -1 && g_tasks[current_task_idx].state == TASK_RUNNABLE) {
        return current_task_idx;
    }
    
    return next_idx;
}

void yield() {
    int next = get_next_task();
    if (next == -1) return;
    if (next == current_task_idx) return;

    int old = current_task_idx;
    current_task_idx = next;
    switch_context(&g_tasks[old].rsp, g_tasks[next].rsp);
}

void scheduler_tick() {
    g_ticks++;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (g_tasks[i].state == TASK_SLEEPING) {
            if (g_tasks[i].sleep_ticks > 0) {
                g_tasks[i].sleep_ticks--;
            } else {
                g_tasks[i].state = TASK_RUNNABLE;
            }
        }
    }
    yield();
}

void task_sleep(uint32_t ticks) {
    g_tasks[current_task_idx].sleep_ticks = ticks;
    g_tasks[current_task_idx].state = TASK_SLEEPING;
    yield();
}