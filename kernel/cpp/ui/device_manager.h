#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "window.h"

class DeviceManager : public Window {
public:
    DeviceManager(int x, int y, int w, int h);
    void Draw(Renderer& r) override;
};

#endif
