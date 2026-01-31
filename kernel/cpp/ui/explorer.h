#ifndef EXPLORER_H
#define EXPLORER_H

#include "window.h"
#include "../../c/fs/ramfs.h"

class FileExplorer : public Window {
public:
    FileExplorer(int x, int y, int w, int h);
    void Draw(Renderer& r) override;
    void OnMouseEvent(int mx, int my, int type) override;
    void HandleKey(uint32_t key_event) override;
    void UpdatePath(const char* new_path);

private:
    char current_path[128];
    RamNode* current_node;
    int selected_index;

    // Navigation History
    char history[16][128];
    int history_ptr;   // Index of current path in history
    int history_count; // Total number of paths in history

    void NavigateTo(RamNode* node);
    void GoBack();
    void GoForward();
};

#endif
