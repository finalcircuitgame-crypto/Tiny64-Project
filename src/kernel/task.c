#include "../include/task.h"
#include "../include/heap.h"
#include <stddef.h>

extern void context_switch(uint64_t* old_rsp, uint64_t new_rsp);

static Task tasks[10];
static int task_count = 0;
static int current_task = 0;

void Task_Init() {
    // Current execution becomes Task 0
    task_count = 1;
    current_task = 0;
}

void Task_Create(void (*entry)(), void* stack_top) {
    if (task_count >= 10) return;
    
    uint64_t* stack = (uint64_t*)stack_top;
    
    // Initial stack frame for context_switch/ret
    *(--stack) = (uint64_t)entry; // RIP
    *(--stack) = 0; // RBP
    *(--stack) = 0; // RBX
    *(--stack) = 0; // R12
    *(--stack) = 0; // R13
    *(--stack) = 0; // R14
    *(--stack) = 0; // R15
    
    tasks[task_count].rsp = (uint64_t)stack;
    task_count++;
}

void Task_Yield() {
    int old_task = current_task;
    current_task = (current_task + 1) % task_count;
    
    context_switch(&tasks[old_task].rsp, tasks[current_task].rsp);
}
