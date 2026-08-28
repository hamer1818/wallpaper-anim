#pragma once

// Linux counterpart of the Windows src/system/system_monitor.* helpers.
namespace SystemMonitor {

    // True when the machine is running from a battery (always false on a desktop).
    bool IsOnBattery();

    // Wayland gives no way to inspect other clients' windows, so "is something
    // fullscreen?" is answered the way the desktop itself answers it: fullscreen
    // players and games hold a power-management / screensaver inhibition while they
    // run. Falls back to false when no inhibition service is reachable.
    bool IsFullscreenAppActive();

}
