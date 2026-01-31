#include "task_manager.h"
#include "../../c/scheduler/scheduler.h"
#include "../../c/lib/string.h"
#include "../../c/graphics/colors.h"

TaskManager::TaskManager(int x, int y, int w, int h)
    : Window("Task Manager", x, y, w, h), selected_task(-1), history_ptr(0) {
    for(int i=0; i<50; i++) cpu_history[i] = 0;
}

void TaskManager::Draw(Renderer& r) {
    if (closed || minimized) return;
    Window::Draw(r);

    // Sidebar/Tabs (Performance vs Processes)
    r.fill_rect(x + 5, y + 35, 100, h - 70, 0xFFF0F0F0);
    r.draw_string("Processes", x + 10, y + 50, COLOR_BLACK, 1);
    r.draw_string("Performance", x + 10, y + 80, 0xFF7F8C8D, 1);

    // Main Headers
    r.fill_rect(x + 105, y + 35, w - 110, 25, 0xFFEEEEEE);
    r.draw_string("Name", x + 115, y + 40, COLOR_BLACK, 1);
    r.draw_string("State", x + 240, y + 40, COLOR_BLACK, 1);
    r.draw_string("Priority", x + 330, y + 40, COLOR_BLACK, 1);

    int current_idx = scheduler_get_current_idx();

    int row_count = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (g_tasks[i].state == TASK_UNUSED) continue;

        int rowY = y + 65 + (row_count * 25);
        if (rowY + 25 > y + h - 100) break;

        // Highlight selected task
        if (i == selected_task) {
            r.fill_rect(x + 105, rowY - 2, w - 110, 22, 0xFFCCE8FF);
        } else if (i == current_idx) {
            r.fill_rect(x + 105, rowY - 2, w - 110, 22, 0xFFD4E6F1);
        }

        r.draw_string(g_tasks[i].name, x + 115, rowY, COLOR_BLACK, 1);

        const char* state_str = "Unknown";
        uint32_t state_col = COLOR_BLACK;
        switch (g_tasks[i].state) {
            case TASK_RUNNABLE: state_str = "Runnable"; state_col = 0xFF229954; break;
            case TASK_BLOCKED:  state_str = "Blocked";  state_col = 0xFFCB4335; break;
            case TASK_SLEEPING: state_str = "Sleeping"; state_col = 0xFFF1C40F; break;
            default: break;
        }
        r.draw_string(state_str, x + 240, rowY, state_col, 1);

        char buf[32];
        ksprintf(buf, "%d", g_tasks[i].priority);
        r.draw_string(buf, x + 330, rowY, COLOR_BLACK, 1);
        row_count++;
    }

    // CPU Graph Area
    int graphX = x + 115;
    int graphY = y + h - 90;
    int graphW = w - 130;
    int graphH = 50;
    r.fill_rect(graphX, graphY, graphW, graphH, 0xFF111111);
    r.draw_string("CPU History", graphX, graphY - 15, 0xFF7F8C8D, 1);

    // Update history (pseudo-random for demo)
    static int tick = 0;
    if (++tick % 10 == 0) {
        static uint32_t seed = 12345;
        seed = seed * 1103515245 + 12345;
        cpu_history[history_ptr] = (seed % 40) + 5; // 5% to 45%
        history_ptr = (history_ptr + 1) % 50;
    }

    // Draw History
    for(int i=0; i<49; i++) {
        int idx1 = (history_ptr + i) % 50;
        int idx2 = (history_ptr + i + 1) % 50;
        int x1 = graphX + (i * graphW / 50);
        int x2 = graphX + ((i + 1) * graphW / 50);
        int y1 = graphY + graphH - cpu_history[idx1];
        int y2 = graphY + graphH - cpu_history[idx2];
        // Line-ish
        r.fill_rect(x1, y1, x2-x1 + 1, 2, 0xFF2ECC71);
    }

    // Bottom Bar
    r.fill_rect(x + 5, y + h - 35, w - 10, 30, 0xFFF5F5F5);
    r.draw_rounded_rect(x + w - 110, y + h - 30, 100, 25, 4, selected_task != -1 ? 0xFFE74C3C : 0xFFBDC3C7);
    r.draw_string("END TASK", x + w - 95, y + h - 25, COLOR_WHITE, 1);
}

void TaskManager::OnMouseEvent(int mx, int my, int type) {
    Window::OnMouseEvent(mx, my, type);
    if (type != MOUSE_EVENT_DOWN) return;

    if (mx >= x + 105 && my >= y + 65 && my <= y + h - 100) {
        int row_idx = (my - (y + 65)) / 25;
        int current_row = 0;
        selected_task = -1;
        for (int i = 0; i < MAX_TASKS; i++) {
            if (g_tasks[i].state != TASK_UNUSED) {
                if (current_row == row_idx) {
                    selected_task = i;
                    break;
                }
                current_row++;
            }
        }
    }

    // End Task button
    if (mx >= x + w - 110 && mx <= x + w - 10 && my >= y + h - 30 && my <= y + h - 5) {
        if (selected_task != -1) {
            // Simulated kill
            if (selected_task != scheduler_get_current_idx()) {
                g_tasks[selected_task].state = TASK_UNUSED;
                selected_task = -1;
            }
        }
    }
}
