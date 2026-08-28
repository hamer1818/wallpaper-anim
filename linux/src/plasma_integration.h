#pragma once

#include <QString>

// KDE Plasma keeps every wlr-layer-shell background surface underneath plasmashell's
// desktop window, and that window paints an opaque background even when its wallpaper
// plugin draws nothing. So on Plasma the only way to appear *behind* the desktop icons
// is to be the wallpaper: this module installs a small QML wallpaper plugin and points
// the desktop containments at it. The plugin reads the same config.json this app writes.
namespace PlasmaIntegration {

    QString PluginId();

    // KDE session and a reachable plasmashell, respectively.
    bool IsPlasmaSession();
    bool IsPlasmaShellRunning();

    // Copies (or refreshes) the wallpaper plugin into ~/.local/share/plasma/wallpapers.
    // Sets *updatedOut when files actually changed, which means a running plasmashell is
    // still holding the previous QML and has to be restarted.
    bool InstallPlugin(QString* errorOut = nullptr, bool* updatedOut = nullptr);

    // Restarts plasmashell so a refreshed plugin package takes effect.
    bool RestartPlasmaShell();

    // Whether the desktop containments currently use our plugin.
    bool IsActive();

    // Points every desktop containment at our plugin / back at org.kde.image.
    bool Activate(QString* errorOut = nullptr);
    bool Restore(QString* errorOut = nullptr);

    // Hands the running plugin its state. QML cannot read arbitrary local files (Qt 6
    // disables XMLHttpRequest on file:// URLs unless QML_XHR_ALLOW_FILE_READ is set,
    // and plasmashell does not set it), so the values go into the plugin's own Plasma
    // configuration through plasmashell's scripting interface.
    bool PushState(const QString& mediaPath, int fitMode, bool paused, QString* errorOut = nullptr);

}
