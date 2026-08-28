#include "app.h"

#include "config.h"
#include "localization.h"
#include "plasma_integration.h"
#include "settings_window.h"
#include "system_monitor.h"
#include "thumbnail.h"
#include "wallpaper_window.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QRandomGenerator>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QTimer>

#include <algorithm>

namespace {

// How long the compositor may leave us without a frame before the wallpaper is
// assumed to be fully covered. Generous, so a stutter never pauses playback.
constexpr qint64 kHiddenThresholdMs = 4000;

QIcon appIcon()
{
    const QStringList candidates = {
        QStringLiteral("/usr/share/icons/hicolor/256x256/apps/wallpaperanim.png"),
        QStringLiteral("/usr/local/share/icons/hicolor/256x256/apps/wallpaperanim.png"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../assets/WallpaperAnim-logo.png"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../assets/WallpaperAnim-logo.png"),
    };
    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) return QIcon(candidate);
    }
    QIcon themed = QIcon::fromTheme(QStringLiteral("wallpaperanim"));
    if (!themed.isNull()) return themed;
    return QIcon::fromTheme(QStringLiteral("preferences-desktop-wallpaper"));
}

} // namespace

App* App::s_instance = nullptr;

App::App(QObject* parent) : QObject(parent)
{
    s_instance = this;
}

App::~App()
{
    destroySurfaces();
    if (s_instance == this) s_instance = nullptr;
}

App* App::Instance() { return s_instance; }

bool App::LayerShellAvailable() const
{
#ifdef WPA_HAVE_LAYER_SHELL
    return true;
#else
    return false;
#endif
}

int App::EffectiveBackend() const
{
    const auto& cfg = Config::ConfigManager::GetInstance().GetConfig();
    int backend = cfg.backend;
    if (backend == Config::BackendAuto) {
        backend = PlasmaIntegration::IsPlasmaSession() ? Config::BackendPlasma : Config::BackendLayerShell;
    }
    if (backend == Config::BackendLayerShell && !LayerShellAvailable()) {
        backend = Config::BackendPlasma;
    }
    return backend;
}

void App::Start(bool showSettingsWindow)
{
    auto& cfg = Config::ConfigManager::GetInstance().GetConfig();

    setupTray();

    m_rotationTimer = new QTimer(this);
    m_rotationTimer->setSingleShot(false);
    connect(m_rotationTimer, &QTimer::timeout, this, &App::onRotationTimeout);

    m_monitorTimer = new QTimer(this);
    m_monitorTimer->setInterval(3000);
    connect(m_monitorTimer, &QTimer::timeout, this, &App::onMonitorTick);
    m_monitorTimer->start();

    connect(qApp, &QGuiApplication::screenAdded, this, &App::onScreenAdded);
    connect(qApp, &QGuiApplication::screenRemoved, this, &App::onScreenRemoved);

    if (EffectiveBackend() == Config::BackendPlasma) {
        // Keep the installed copy of the QML plugin current, but never hijack the
        // desktop wallpaper on startup: only an explicit action in the UI does that.
        QString installError;
        bool packageUpdated = false;
        if (!PlasmaIntegration::InstallPlugin(&installError, &packageUpdated)) {
            qWarning() << "WallpaperAnim: Plasma plugin install failed:" << installError;
        } else if (packageUpdated && PlasmaIntegration::IsActive()) {
            // The shell is still running the previous QML; only a restart picks up the
            // refreshed package.
            qWarning() << "WallpaperAnim: Plasma wallpaper plugin updated, restarting plasmashell";
            PlasmaIntegration::RestartPlasmaShell();
        }
    }

    RebuildSurfaces();
    RefreshRotationTimer();

    if (!cfg.lastVideoPath.empty()) {
        const QString path = QString::fromStdString(cfg.lastVideoPath);
        if (QFileInfo::exists(path)) {
            QString error;
            for (WallpaperWindow* window : std::as_const(m_windows)) {
                window->setMedia(path, &error);
            }
        }
    }

    PushPlasmaState();
    // Materialise config.json on first run.
    Config::ConfigManager::GetInstance().Save();
    m_started = true;

    if (showSettingsWindow || cfg.isFirstRun) {
        ShowSettings();
    }
}

void App::setupTray()
{
    const auto& strings = Localization::Get();

    m_trayMenu = new QMenu();
    m_pauseAction = m_trayMenu->addAction(QString::fromUtf8(strings.trayPause));
    connect(m_pauseAction, &QAction::triggered, this, [this]() { SetUserPaused(!m_userPaused); });

    m_nextAction = m_trayMenu->addAction(QString::fromUtf8(strings.trayNext));
    connect(m_nextAction, &QAction::triggered, this, &App::NextWallpaper);

    m_trayMenu->addSeparator();

    m_settingsAction = m_trayMenu->addAction(QString::fromUtf8(strings.traySettings));
    connect(m_settingsAction, &QAction::triggered, this, &App::ShowSettings);

    m_quitAction = m_trayMenu->addAction(QString::fromUtf8(strings.trayQuit));
    connect(m_quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    m_tray = new QSystemTrayIcon(appIcon(), this);
    m_tray->setToolTip(QString::fromUtf8(strings.trayTooltip));
    m_tray->setContextMenu(m_trayMenu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) ShowSettings();
    });
    m_tray->show();
}

void App::updateTrayState()
{
    if (!m_pauseAction) return;
    const auto& strings = Localization::Get();
    m_pauseAction->setText(m_userPaused ? QString::fromUtf8(strings.trayResume)
                                        : QString::fromUtf8(strings.trayPause));
}

void App::destroySurfaces()
{
    for (WallpaperWindow* window : std::as_const(m_windows)) {
        window->close();
        delete window;
    }
    m_windows.clear();
}

void App::RebuildSurfaces()
{
    destroySurfaces();

    if (EffectiveBackend() != Config::BackendLayerShell) {
        // The Plasma backend renders inside plasmashell; no surfaces of our own.
        return;
    }

    const auto& cfg = Config::ConfigManager::GetInstance().GetConfig();
    const QString path = QString::fromStdString(cfg.lastVideoPath);

    const QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        auto* window = new WallpaperWindow(screen, cfg.layer);
        connect(window, &WallpaperWindow::mediaFailed, this, &App::onMediaFailed);
        window->setFitMode(cfg.fitMode);
        window->setMaxFps(cfg.maxFPS);
        window->setHardwareDecode(cfg.hardwareDecode);
        window->setVolume(cfg.volume);
        window->resize(screen->size());
        window->show();
        if (!path.isEmpty() && QFileInfo::exists(path)) window->setMedia(path);
        window->setPaused(m_userPaused || m_autoPaused);
        m_windows.append(window);
    }
}

void App::onScreenAdded(QScreen* screen)
{
    Q_UNUSED(screen);
    if (EffectiveBackend() == Config::BackendLayerShell) RebuildSurfaces();
}

void App::onScreenRemoved(QScreen* screen)
{
    Q_UNUSED(screen);
    if (EffectiveBackend() == Config::BackendLayerShell) RebuildSurfaces();
}

bool App::ApplyWallpaper(const QString& path, QString* errorOut)
{
    if (path.isEmpty()) return false;
    if (!QFileInfo::exists(path)) {
        if (errorOut) *errorOut = QStringLiteral("File not found: %1").arg(path);
        return false;
    }

    auto& manager = Config::ConfigManager::GetInstance();
    auto& cfg = manager.GetConfig();

    QString loadError;
    bool ok = true;
    for (WallpaperWindow* window : std::as_const(m_windows)) {
        if (!window->setMedia(path, &loadError)) ok = false;
    }
    if (!ok) {
        if (errorOut) *errorOut = loadError;
        return false;
    }

    cfg.lastVideoPath = path.toStdString();
    addToHistory(path, QFileInfo(path).completeBaseName(), Config::MediaTypeForPath(path.toStdString()));
    manager.Save();

    // The Plasma wallpaper plugin follows config.json rather than a live connection.
    if (EffectiveBackend() == Config::BackendPlasma) {
        // Picking a wallpaper *is* the request to show it, so take over the desktop
        // containment here. (Startup deliberately does not: see App::Start.)
        if (!PlasmaIntegration::IsActive()) {
            QString activateError;
            if (!PlasmaIntegration::Activate(&activateError)) {
                if (errorOut) {
                    *errorOut = activateError.isEmpty()
                                    ? QStringLiteral("Could not set the Plasma wallpaper")
                                    : activateError;
                }
                Q_EMIT wallpaperChanged(path);
                return false;
            }
        }
        PushPlasmaState();
    }

    Q_EMIT wallpaperChanged(path);
    return true;
}

void App::addToHistory(const QString& path, const QString& displayName, int type)
{
    auto& cfg = Config::ConfigManager::GetInstance().GetConfig();
    const std::string utf8Path = path.toStdString();

    auto existing = std::find_if(cfg.history.begin(), cfg.history.end(),
                                 [&](const Config::WallpaperHistoryItem& item) {
                                     return item.path == utf8Path;
                                 });
    if (existing != cfg.history.end()) {
        // Move it to the front so the library keeps most-recent-first order.
        Config::WallpaperHistoryItem item = *existing;
        cfg.history.erase(existing);
        cfg.history.insert(cfg.history.begin(), std::move(item));
        return;
    }

    Config::WallpaperHistoryItem item;
    item.path = utf8Path;
    item.name = displayName.toStdString();
    item.type = type;
    item.thumbPath = Thumbnail::Generate(path).toStdString();
    cfg.history.insert(cfg.history.begin(), std::move(item));
}

QString App::CurrentWallpaper() const
{
    return QString::fromStdString(Config::ConfigManager::GetInstance().GetConfig().lastVideoPath);
}

void App::SetUserPaused(bool paused)
{
    if (m_userPaused == paused) return;
    m_userPaused = paused;
    applyPauseState();
    updateTrayState();
    Q_EMIT pausedChanged(paused);
}

void App::applyPauseState()
{
    const bool paused = m_userPaused || m_autoPaused;
    for (WallpaperWindow* window : std::as_const(m_windows)) {
        window->setPaused(paused);
    }
    PushPlasmaState();
}

void App::PushPlasmaState()
{
    if (EffectiveBackend() != Config::BackendPlasma) return;
    if (!PlasmaIntegration::IsActive()) return;

    const auto& cfg = Config::ConfigManager::GetInstance().GetConfig();
    PlasmaIntegration::PushState(QString::fromStdString(cfg.lastVideoPath), cfg.fitMode,
                                 m_userPaused || m_autoPaused);
}

void App::ApplyPlaybackSettings()
{
    const auto& cfg = Config::ConfigManager::GetInstance().GetConfig();
    for (WallpaperWindow* window : std::as_const(m_windows)) {
        window->setFitMode(cfg.fitMode);
        window->setMaxFps(cfg.maxFPS);
        window->setHardwareDecode(cfg.hardwareDecode);
        window->setVolume(cfg.volume);
    }
    PushPlasmaState();
}

void App::RefreshRotationTimer()
{
    const auto& cfg = Config::ConfigManager::GetInstance().GetConfig();
    if (!m_rotationTimer) return;

    if (!cfg.playlistEnabled) {
        m_rotationTimer->stop();
        return;
    }
    const int minutes = qMax(1, cfg.playlistIntervalMin);
    m_rotationTimer->start(minutes * 60 * 1000);
}

QStringList App::rotationCandidates() const
{
    const auto& cfg = Config::ConfigManager::GetInstance().GetConfig();
    QStringList candidates;

    if (!cfg.activePlaylist.empty()) {
        for (const auto& playlist : cfg.playlists) {
            if (playlist.name != cfg.activePlaylist) continue;
            for (const auto& path : playlist.paths) {
                const QString qpath = QString::fromStdString(path);
                if (QFileInfo::exists(qpath)) candidates << qpath;
            }
            break;
        }
    }

    if (candidates.isEmpty()) {
        for (const auto& item : cfg.history) {
            const QString qpath = QString::fromStdString(item.path);
            if (QFileInfo::exists(qpath)) candidates << qpath;
        }
    }
    return candidates;
}

void App::onRotationTimeout()
{
    NextWallpaper();
}

void App::NextWallpaper()
{
    const QStringList candidates = rotationCandidates();
    if (candidates.size() < 2) return;

    const auto& cfg = Config::ConfigManager::GetInstance().GetConfig();
    const QString current = QString::fromStdString(cfg.lastVideoPath);

    QString next;
    if (cfg.playlistShuffle) {
        // Avoid repeating the current wallpaper when there is an alternative.
        for (int attempt = 0; attempt < 8; ++attempt) {
            next = candidates.at(QRandomGenerator::global()->bounded(candidates.size()));
            if (next != current) break;
        }
    } else {
        int index = candidates.indexOf(current);
        index = (index < 0) ? 0 : (index + 1) % candidates.size();
        next = candidates.at(index);
    }

    if (!next.isEmpty() && next != current) ApplyWallpaper(next);
}

void App::onMonitorTick()
{
    const auto& cfg = Config::ConfigManager::GetInstance().GetConfig();

    bool shouldPause = false;
    if (cfg.pauseOnBattery && SystemMonitor::IsOnBattery()) shouldPause = true;
    if (!shouldPause && cfg.pauseOnFullscreen && SystemMonitor::IsFullscreenAppActive()) shouldPause = true;

    if (!shouldPause && cfg.pauseWhenHidden && !m_userPaused && !m_windows.isEmpty()) {
        bool allStale = true;
        for (WallpaperWindow* window : std::as_const(m_windows)) {
            if (window->msSinceLastPaint() < kHiddenThresholdMs) {
                allStale = false;
                break;
            }
        }
        shouldPause = allStale;
    }

    // Keep asking for a frame: the request only lands when the compositor considers
    // the surface visible again, which is how a hidden-pause is lifted.
    if (m_autoPaused) {
        for (WallpaperWindow* window : std::as_const(m_windows)) {
            window->requestUpdate();
        }
    }

    if (shouldPause != m_autoPaused) {
        m_autoPaused = shouldPause;
        applyPauseState();
    }
}

void App::onMediaFailed(const QString& message)
{
    qWarning() << "WallpaperAnim: media error:" << message;
    if (m_tray && m_tray->isVisible()) {
        const auto& strings = Localization::Get();
        m_tray->showMessage(QString::fromUtf8(strings.mediaLoadFailedTitle), message,
                            QSystemTrayIcon::Warning, 5000);
    }
}

void App::ShowSettings()
{
    if (!m_settings) {
        m_settings = new SettingsWindow(this);
    }
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}
