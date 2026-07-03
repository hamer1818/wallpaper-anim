#pragma once

#include <windows.h>

namespace SystemMonitor {
    // Checks if the system is running on battery power
    bool IsOnBattery();

    // Checks if there is a fullscreen application currently active (e.g. game)
    bool IsFullscreenAppActive();

    // True when the whole desktop is hidden and the wallpaper cannot possibly be seen,
    // so decoding/rendering it is pure wasted CPU. Single-monitor only: a maximized
    // foreground window fully covers the wallpaper there. On multi-monitor setups this
    // returns false (a maximized window on one screen leaves the wallpaper visible on the
    // others) — use IsFullscreenAppActive() for the exclusive-fullscreen case instead.
    bool IsDesktopCovered();
}
