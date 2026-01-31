#include "settings.h"
#include "../../c/drivers/pci/pci.h"
#include "../../c/graphics/colors.h"
#include "../../c/memory/pmm.h"
#include "../utils/hardware.h"
#include "../utils/persistence.h"
#include "app_icons.h"

extern "C" void _optimize_system();

SettingsWindow::SettingsWindow(int x, int y, int w, int h)
    : Window("Settings", x, y, w, h) {
  active_tab = 0;
  selected_wallpaper = Persistence::LoadWallpaper();
  selected_theme = Persistence::LoadTheme();
  strcpy(username, Persistence::LoadUsername());
  hardware_accel = true;
  show_fps = false;
}

void SettingsWindow::Draw(Renderer &r) {
  Window::Draw(r);
  if (minimized || closed)
    return;

  int sidebar_w = 120;
  int content_x = x + sidebar_w + 5;
  int content_y = y + 35;
  int content_w = w - sidebar_w - 15;
  int content_h = h - 45;

  // Draw a neutral gray background and a slightly lighter gray sidebar.
  // Colors use 0xAARRGGBB (alpha, red, green, blue).
  // Adjust the hex values if you want lighter/darker grays.

  // Sidebar background (inset, slightly lighter)
  r.fill_rect(x + 5, y + 35, sidebar_w, h - 45, 0xFF3A3A3A);

  const char *tabs[] = {"About",  "Display", "Personalize",
                        "System", "Network", "Security"};
  for (int i = 0; i < 6; i++) {
    // Active tab highlight: a lighter gray with some transparency
    if (active_tab == i)
      r.fill_rect(x + 5, y + 40 + i * 40, sidebar_w, 35, 0x88484848);

    // Tab text: keep it readable on gray (white)
    r.draw_string(tabs[i], x + 15, y + 50 + i * 40, 0xFFFFFFFF, 1);
  }

  // Optional: subtle divider between sidebar and content
  r.fill_rect(x + 5 + sidebar_w, y + 35, 1, h - 45, 0xFF262626);

  // Content Area
  r.draw_rounded_rect(content_x, content_y, content_w, content_h, 5,
                      0x11FFFFFF);

  if (active_tab == 0) { // About
    r.draw_string("TIdfdfdfdfdfdNY64 OS", content_x + 20, content_y + 20,
                  0xFF3498DB, 2);
    r.draw_string("Version 1.3.5 Platinum", content_x + 20, content_y + 50,
                  0xFFAAAAAA, 1);
    r.draw_string("Kernel: 64-bit Microkernel", content_x + 20, content_y + 80,
                  0xFFFFFFFF, 1);
    r.draw_string("Developer: Gemini CLI Agent", content_x + 20,
                  content_y + 110, 0xFFFFFFFF, 1);
    r.draw_string("Special Thanks: stb, Limine, OVMF", content_x + 20,
                  content_y + 140, 0xFFAAAAAA, 1);
  } else if (active_tab == 1) { // Display
    r.draw_string("Display Settings", content_x + 20, content_y + 20,
                  0xFFFFFFFF, 1);

    r.draw_string("Hardware Acceleration", content_x + 20, content_y + 50,
                  0xFFFFFFFF, 1);
    r.draw_rounded_rect(content_x + 220, content_y + 45, 50, 25, 12,
                        hardware_accel ? 0xFF2ECC71 : 0xFF7F8C8D);
    r.fill_rect(content_x + (hardware_accel ? 245 : 225), content_y + 48, 20,
                19, 0xFFFFFFFF);

    r.draw_string("Show FPS Counter", content_x + 20, content_y + 90,
                  0xFFFFFFFF, 1);
    r.draw_rounded_rect(content_x + 220, content_y + 85, 50, 25, 12,
                        show_fps ? 0xFF2ECC71 : 0xFF7F8C8D);
    r.fill_rect(content_x + (show_fps ? 245 : 225), content_y + 88, 20, 19,
                0xFFFFFFFF);

    r.draw_string("Resolution: 1024x768 (Auto)", content_x + 20,
                  content_y + 140, 0xFFAAAAAA, 1);
  } else if (active_tab == 2) { // Personalize
    r.draw_string("Personalization", content_x + 20, content_y + 20, 0xFFFFFFFF,
                  1);

    r.draw_string("User Name", content_x + 20, content_y + 45, 0xFFAAAAAA, 1);
    r.draw_rounded_rect(content_x + 20, content_y + 60, 150, 25, 4, 0x22FFFFFF);
    r.draw_string(username, content_x + 28, content_y + 65, 0xFFFFFFFF, 1);

    r.draw_string("Wallpaper", content_x + 20, content_y + 100, 0xFFAAAAAA, 1);
    const char *wps[] = {"Tiny Blue", "Deep Space", "Modern Dark"};
    for (int i = 0; i < 3; i++) {
      bool sel = (selected_wallpaper == i);
      r.draw_rounded_rect(content_x + 20, content_y + 120 + i * 35, 150, 30, 5,
                          sel ? 0x883498DB : 0x44FFFFFF);
      r.draw_string(wps[i], content_x + 30, content_y + 128 + i * 35,
                    0xFFFFFFFF, 1);
    }

    r.draw_string("Theme", content_x + 200, content_y + 100, 0xFFAAAAAA, 1);
    const char *themes[] = {"Dark", "Light", "Platinum"};
    for (int i = 0; i < 3; i++) {
      bool sel = (selected_theme == i);
      r.draw_rounded_rect(content_x + 200, content_y + 120 + i * 35, 150, 30, 5,
                          sel ? 0x883498DB : 0x44FFFFFF);
      r.draw_string(themes[i], content_x + 210, content_y + 128 + i * 35,
                    0xFFFFFFFF, 1);
    }
  } else if (active_tab == 3) { // System
    r.draw_string("System Information", content_x + 20, content_y + 20,
                  0xFFFFFFFF, 1);

    Hardware::CPUInfo info = Hardware::GetCPUInfo();
    r.draw_string("CPU:", content_x + 20, content_y + 50, 0xFF3498DB, 1);
    r.draw_string(info.brand, content_x + 60, content_y + 50, 0xFFFFFFFF, 1);

    uint64_t total_mem = pmm_get_total_memory();
    uint64_t free_mem = pmm_get_free_memory();
    char mbuf[128];
    ksprintf(mbuf, "Memory: %d MB Total / %d MB Free",
             (int)(total_mem / 1024 / 1024), (int)(free_mem / 1024 / 1024));
    r.draw_string(mbuf, content_x + 20, content_y + 80, 0xFFFFFFFF, 1);

    r.draw_string("GPU: Tiny64 Generic VBE", content_x + 20, content_y + 110,
                  0xFFFFFFFF, 1);

    r.draw_rounded_rect(content_x + 20, content_y + 160, 180, 35, 6,
                        0xFF3498DB);
    r.draw_string("OPTIMIZE SYSTEM", content_x + 35, content_y + 170,
                  0xFFFFFFFF, 1);
  } else if (active_tab == 4) { // Network
    r.draw_string("Network Configuration", content_x + 20, content_y + 20,
                  0xFFFFFFFF, 1);

    // Draw Ethernet Icon
    r.draw_image(icon_ethernet_data, content_x + 20, content_y + 50, 32, 32);

    r.draw_string("Adapter: Intel e1000 (Detected)", content_x + 60,
                  content_y + 55, 0xFF00FF00, 1);
    r.draw_string("IP Address: 172.16.6.222", content_x + 20, content_y + 90,
                  0xFFFFFFFF, 1);
    r.draw_string("Gateway: 172.16.6.1", content_x + 20, content_y + 110,
                  0xFFFFFFFF, 1);
    r.draw_string("Status: Connected", content_x + 20, content_y + 140,
                  0xFF00FF00, 1);

    r.draw_rounded_rect(content_x + 20, content_y + 170, 150, 30, 5,
                        0x44FFFFFF);
    r.draw_string("Renew IP", content_x + 55, content_y + 178, 0xFFFFFFFF, 1);
  } else if (active_tab == 5) { // Security
    r.draw_string("Security & Privacy", content_x + 20, content_y + 20,
                  0xFFFFFFFF, 1);

    r.draw_string("Firewall Status:", content_x + 20, content_y + 50,
                  0xFFFFFFFF, 1);
    r.draw_string("ACTIVE", content_x + 150, content_y + 50, 0xFF2ECC71, 1);

    r.draw_string("Encrypted FS:", content_x + 20, content_y + 80, 0xFFFFFFFF,
                  1);
    r.draw_string("DISABLED", content_x + 150, content_y + 80, 0xFFE74C3C, 1);

    r.draw_string("Kernel Isolation:", content_x + 20, content_y + 110,
                  0xFFFFFFFF, 1);
    r.draw_string("LEVEL 3 (High)", content_x + 150, content_y + 110,
                  0xFF3498DB, 1);

    r.draw_rounded_rect(content_x + 20, content_y + 160, 200, 35, 6,
                        0xFFE67E22);
    r.draw_string("RESET SECURITY", content_x + 40, content_y + 170, 0xFFFFFFFF,
                  1);
  }
}

void SettingsWindow::OnMouseEvent(int mx, int my, int type) {
  Window::OnMouseEvent(mx, my, type);
  if (type != MOUSE_EVENT_UP)
    return;

  int sidebar_w = 120;
  // Tab switching
  if (mx >= x + 5 && mx <= x + sidebar_w + 5) {
    for (int i = 0; i < 6; i++) {
      if (my >= y + 40 + i * 40 && my <= y + 75 + i * 40) {
        active_tab = i;
        return;
      }
    }
  }

  int content_x = x + sidebar_w + 5;
  int content_y = y + 35;

  if (active_tab == 1) { // Display
    if (mx >= content_x + 220 && mx <= content_x + 270) {
      if (my >= content_y + 45 && my <= content_y + 70)
        hardware_accel = !hardware_accel;
      if (my >= content_y + 85 && my <= content_y + 110)
        show_fps = !show_fps;
    }
  } else if (active_tab == 2) { // Personalize
    // Wallpaper
    if (mx >= content_x + 20 && mx <= content_x + 170) {
      for (int i = 0; i < 3; i++) {
        if (my >= content_y + 120 + i * 35 && my <= content_y + 150 + i * 35) {
          selected_wallpaper = i;
          Persistence::SaveWallpaper(i);
        }
      }
    }
    // Theme
    if (mx >= content_x + 200 && mx <= content_x + 350) {
      for (int i = 0; i < 3; i++) {
        if (my >= content_y + 120 + i * 35 && my <= content_y + 150 + i * 35) {
          selected_theme = i;
          Persistence::SaveTheme(i);
        }
      }
    }
  } else if (active_tab == 3) { // System
    if (mx >= content_x + 20 && mx <= content_x + 200 &&
        my >= content_y + 160 && my <= content_y + 195) {
      _optimize_system();
    }
  }
}