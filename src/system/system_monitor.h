#pragma once

#include <windows.h>

namespace SystemMonitor {
    // Checks if the system is running on battery power
    bool IsOnBattery();

    // Checks if there is a fullscreen application currently active (e.g. game)
    bool IsFullscreenAppActive();
}
