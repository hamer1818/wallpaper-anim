#pragma once

#include <QList>
#include <QObject>
#include <QString>

class QSystemTrayIcon;
class QMenu;
class QAction;
class QTimer;
class QScreen;
class WallpaperWindow;
class SettingsWindow;

// Linux counterpart of App.xaml.cpp: owns the wallpaper surfaces, the tray icon,
// auto-rotation and the auto-pause policy, and keeps the settings UI in sync.
class App : public QObject {
    Q_OBJECT

public:
    explicit App(QObject* parent = nullptr);
    ~App() override;

    static App* Instance();

    void Start(bool showSettingsWindow);

    // Applies a wallpaper, adds it to the library and persists the config.
    bool ApplyWallpaper(const QString& path, QString* errorOut = nullptr);
    void NextWallpaper();

    void SetUserPaused(bool paused);
    bool IsUserPaused() const { return m_userPaused; }

    // Re-reads the live config into the running surfaces (fit mode, fps, ...).
    void ApplyPlaybackSettings();
    // Publishes the current wallpaper/fit/pause state to the Plasma wallpaper plugin.
    void PushPlasmaState();
    // Records that the user handed the Plasma desktop to us (or took it back), which
    // is what allows the watchdog to re-take it after plasmashell drops the plugin.
    void SetPlasmaOwnership(bool owned);
    // Recreates the wallpaper surfaces after a backend/layer/screen change.
    void RebuildSurfaces();
    void RefreshRotationTimer();

    // Resolves Config::BackendAuto to a concrete backend for this session.
    int EffectiveBackend() const;
    bool LayerShellAvailable() const;

    void ShowSettings();
    QString CurrentWallpaper() const;

Q_SIGNALS:
    void wallpaperChanged(const QString& path);
    void pausedChanged(bool paused);

private Q_SLOTS:
    void onScreenAdded(QScreen* screen);
    void onScreenRemoved(QScreen* screen);
    void onRotationTimeout();
    void onMonitorTick();
    void onMediaFailed(const QString& message);

private:
    void setupTray();
    void updateTrayState();
    void destroySurfaces();
    void applyPauseState();
    void addToHistory(const QString& path, const QString& displayName, int type);
    QStringList rotationCandidates() const;
    void reassertPlasmaWallpaper();

    static App* s_instance;

    QList<WallpaperWindow*> m_windows;
    SettingsWindow* m_settings = nullptr;
    QSystemTrayIcon* m_tray = nullptr;
    QMenu* m_trayMenu = nullptr;
    QAction* m_pauseAction = nullptr;
    QAction* m_nextAction = nullptr;
    QAction* m_settingsAction = nullptr;
    QAction* m_quitAction = nullptr;

    QTimer* m_rotationTimer = nullptr;
    QTimer* m_monitorTimer = nullptr;

    bool m_userPaused = false;
    bool m_autoPaused = false;
    int m_rotationIndex = 0;
    bool m_started = false;
    // Monitor ticks since the last Plasma ownership check; the check costs a blocking
    // D-Bus round trip, so it runs far less often than the 3 s pause polling.
    int m_plasmaCheckTicks = 0;
};
