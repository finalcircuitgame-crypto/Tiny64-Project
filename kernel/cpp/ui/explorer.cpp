#include "../../c/drivers/hid/input.h"
#include "explorer.h"
#include "../../c/graphics/colors.h"
#include "../../c/lib/string.h"

FileExplorer::FileExplorer(int x, int y, int w, int h)
    : Window("File Explorer", x, y, w, h), selected_index(-1), history_ptr(0), history_count(1) {
  strcpy(current_path, "/Downloads");
  
  // Find Downloads node
  current_node = ramfs_get_root();
  RamNode* node = current_node->child;
  while (node) {
      if (strcmp(node->name, "Downloads") == 0) {
          current_node = node;
          break;
      }
      node = node->next;
  }
  
  strcpy(history[0], current_path);
}

void FileExplorer::NavigateTo(RamNode* node) {
    if (!node || node->type != FS_DIR) return;
    
    current_node = node;
    
    // Update path
    if (node == ramfs_get_root()) {
        strcpy(current_path, "/");
    } else {
        // Simple path reconstruction (might be improved)
        strcpy(current_path, "/");
        strcat(current_path, node->name);
    }

    // Add to history
    if (history_ptr < 15) {
        history_ptr++;
        strcpy(history[history_ptr], current_path);
        history_count = history_ptr + 1;
    }
    selected_index = -1;
}

void FileExplorer::GoBack() {
    if (history_ptr > 0) {
        history_ptr--;
        // For now, we need to find the node again because ramfs is flat-ish
        // and we only store the path string in history.
        // In this simple ramfs, we can find it by name.
        RamNode* root = ramfs_get_root();
        if (strcmp(history[history_ptr], "/") == 0) {
            current_node = root;
        } else {
            RamNode* node = root->child;
            while (node) {
                // Path in history starts with /, node->name doesn't?
                // Wait, ramfs_create("Tiny64/drivers", FS_DIR)
                // So history[history_ptr] is "/Tiny64/drivers"
                // node->name might be "Tiny64/drivers"
                if (strcmp(history[history_ptr] + 1, node->name) == 0) {
                    current_node = node;
                    break;
                }
                node = node->next;
            }
        }
        strcpy(current_path, history[history_ptr]);
        selected_index = -1;
    }
}

void FileExplorer::GoForward() {
    if (history_ptr < history_count - 1) {
        history_ptr++;
        RamNode* root = ramfs_get_root();
        if (strcmp(history[history_ptr], "/") == 0) {
            current_node = root;
        } else {
            RamNode* node = root->child;
            while (node) {
                if (strcmp(history[history_ptr] + 1, node->name) == 0) {
                    current_node = node;
                    break;
                }
                node = node->next;
            }
        }
        strcpy(current_path, history[history_ptr]);
        selected_index = -1;
    }
}

void FileExplorer::HandleKey(uint32_t key_event) {
    // CTRL + A (A is scancode 0x1E, which is 'a' or 'A')
    uint8_t c = (uint8_t)(key_event & 0xFF);
    if ((key_event & KB_MOD_CTRL) && (c == 'a' || c == 'A')) {
        selected_index = -2; // Special value for "Select All"
    }
}

void FileExplorer::Draw(Renderer &r) {
  if (closed || minimized)
    return;
  Window::Draw(r); // Draw frame

  // Navigation Buttons
  // Back
  r.draw_rounded_rect(x + 10, y + 40, 25, 25, 4, history_ptr > 0 ? 0xFFDDDDDD : 0xFFEEEEEE);
  r.draw_string("<", x + 18, y + 46, history_ptr > 0 ? COLOR_BLACK : 0xFFAAAAAA, 1);
  
  // Forward
  r.draw_rounded_rect(x + 40, y + 40, 25, 25, 4, history_ptr < history_count - 1 ? 0xFFDDDDDD : 0xFFEEEEEE);
  r.draw_string(">", x + 48, y + 46, history_ptr < history_count - 1 ? COLOR_BLACK : 0xFFAAAAAA, 1);

  // New Folder Button
  r.draw_rounded_rect(x + w - 80, y + 40, 70, 25, 4, 0xFF3498DB);
  r.draw_string("New", x + w - 70, y + 46, COLOR_WHITE, 1);

  // Address Bar
  r.draw_rounded_rect(x + 75, y + 40, w - 160, 25, 4, 0xFFEEEEEE);
  r.draw_string(current_path, x + 85, y + 46, COLOR_BLACK, 1);

  // Sidebar (Shortcuts - No C:)
  r.fill_rect(x + 4, y + 70, 120, h - 75, 0xFFF5F5F5);
  const char *shortcuts[] = {"Quick Access", "System Root", "Network",
                             "User Data"};
  for (int i = 0; i < 4; i++) {
    r.draw_string(shortcuts[i], x + 12, y + 85 + (i * 25), COLOR_BLACK, 1);
  }

  // Main Content Area
  r.fill_rect(x + 125, y + 70, w - 130, h - 75, COLOR_WHITE);

  // List Files
  if (current_node) {
    RamNode *node = current_node->child;
    int i = 0;
    while (node) {
      int itemY = y + 80 + (i * 30);
      if (itemY + 30 > y + h)
        break;

      if (selected_index == i || selected_index == -2) {
        r.fill_rect(x + 130, itemY - 5, w - 145, 25, 0xFFCCE8FF);
      }

      // Icon (Simplified)
      if (node->type == FS_DIR) {
        // Folder body
        r.fill_rect(x + 140, itemY + 3, 16, 10, 0xFFFFD700); // main body

        // Folder tab
        r.fill_rect(x + 140, itemY, 10, 5, 0xFFFFE380); // lighter tab

        // Shadow line (bottom)
        r.fill_rect(x + 140, itemY + 12, 16, 2, 0xFFB8860B);
      } else {
        // File body
        r.fill_rect(x + 140, itemY, 12, 16, 0xFFE0E0E0);

        // Folded corner
        r.fill_rect(x + 140 + 8, itemY, 4, 4, 0xFFFFFFFF);

        // Shading under the fold
        r.fill_rect(x + 140 + 8, itemY + 2, 4, 2, 0xFFCCCCCC);

        // Left border for depth
        r.fill_rect(x + 140, itemY, 1, 16, 0xFFAAAAAA);
      }

      r.draw_string(node->name, x + 165, itemY, COLOR_BLACK, 1);

      node = node->next;
      i++;
    }
  }
}

void FileExplorer::OnMouseEvent(int mx, int my, int type) {
  Window::OnMouseEvent(mx, my, type);

  if (type == MOUSE_EVENT_DOWN) {
    // Back button
    if (mx >= x + 10 && mx <= x + 35 && my >= y + 40 && my <= y + 65) {
        GoBack();
        return;
    }
    // Forward button
    if (mx >= x + 40 && mx <= x + 65 && my >= y + 40 && my <= y + 65) {
        GoForward();
        return;
    }

    // New Folder
    if (mx >= x + w - 80 && mx <= x + w - 10 && my >= y + 40 && my <= y + 65) {
        if (current_node) {
            char path[256];
            strcpy(path, current_path);
            if (path[strlen(path)-1] != '/') strcat(path, "/");
            strcat(path, "New Folder");
            ramfs_create(path, FS_DIR);
        }
        return;
    }

    if (mx >= x + 130 && mx <= x + w - 15 && my >= y + 75) {
      int idx = (my - (y + 75)) / 30;

      // Basic selection and opening logic
      RamNode *node = current_node->child;
      int i = 0;
      while (node && i < idx) {
        node = node->next;
        i++;
      }

      if (node && i == idx) {
        if (selected_index == idx) {
          // Double click (simulated)
          if (node->type == FS_DIR) {
            NavigateTo(node);
          }
        }
        selected_index = idx;
      } else {
          selected_index = -1;
      }
    }
  }
}