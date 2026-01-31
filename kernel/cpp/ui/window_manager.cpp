#include "window_manager.h"

WindowManager::WindowManager() : window_count(0), focused_idx(-1) {}

void WindowManager::AddWindow(Window* win) {
    if (window_count < MAX_WINDOWS) {
        windows[window_count++] = win;
        focused_idx = window_count - 1;
    }
}

void WindowManager::BringToFront(int idx) {
    if (idx < 0 || idx >= window_count || idx == window_count - 1) return;
    
    Window* tmp = windows[idx];
    for (int i = idx; i < window_count - 1; i++) {
        windows[i] = windows[i+1];
    }
    windows[window_count - 1] = tmp;
    focused_idx = window_count - 1;
}

void WindowManager::DrawAll(Renderer& r) {
    for (int i = 0; i < window_count; i++) {
        if (!windows[i]->IsClosed() && !windows[i]->IsMinimized()) {
            windows[i]->Tick();
        }
        windows[i]->Draw(r);
    }
}

void WindowManager::HandleMouse(int mx, int my, int event_type) {
    // Pass event to focused window first if it's a move or release, 
    // unless we clicked a new window
    
    if (event_type == MOUSE_EVENT_DOWN) {
        // Check from top to bottom for focus
        for (int i = window_count - 1; i >= 0; i--) {
            Window* win = windows[i];
            if (win->IsClosed() || win->IsMinimized()) continue;

            if (mx >= win->GetX() && mx <= win->GetX() + win->GetW() &&
                my >= win->GetY() && my <= win->GetY() + win->GetH()) {
                
                BringToFront(i);
                // Dispatch Down event to the now-focused window
                windows[window_count - 1]->OnMouseEvent(mx, my, MOUSE_EVENT_DOWN);
                return;
            }
        }
    }

    // If we are here, either it wasn't a DOWN event, or we clicked background (no window hit)
    // Dispatch to focused window (if any)
    if (focused_idx != -1) {
        // Don't send DOWN event if we clicked background (handled above)
        // But do send MOVE and UP
        if (event_type != MOUSE_EVENT_DOWN) {
             windows[focused_idx]->OnMouseEvent(mx, my, event_type);
        } else {
             // If we clicked background, focused window might need to know? 
             // Or maybe lose focus? For now, just do nothing or let it know?
             // Actually, if we click outside, maybe we should stop dragging if it was happening?
             // But Dragging is usually matched with Up.
        }
    }
}

void WindowManager::HandleKey(uint32_t key_event) {
    if (window_count > 0) {
        windows[window_count - 1]->HandleKey(key_event);
    }
}

Window* WindowManager::GetWindowByTitle(const char* title) {
    for (int i = 0; i < window_count; i++) {
        // Simple strcmp
        const char* t1 = windows[i]->GetTitle();
        const char* t2 = title;
        bool match = true;
        while (*t1 && *t2) {
            if (*t1 != *t2) { match = false; break; }
            t1++; t2++;
        }
        if (match && *t1 == *t2) return windows[i];
    }
    return nullptr;
}
