#pragma once

#include <QString>

// XDG autostart entry, the Linux stand-in for the Windows HKCU ...\Run value.
namespace Autostart {

    bool IsEnabled();
    bool SetEnabled(bool enabled, QString* errorOut = nullptr);

}
