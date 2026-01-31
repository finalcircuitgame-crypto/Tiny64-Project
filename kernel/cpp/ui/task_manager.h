#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "window.h"

class TaskManager : public Window {
public:
    TaskManager(int x, int y, int w, int h);
    void Draw(Renderer& r) override;
    void OnMouseEvent(int mx, int my, int type) override;

private:
    int selected_task;
    int cpu_history[50];
    int history_ptr;
};

#endif
