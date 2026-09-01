#include "plasma_integration.h"

#include "config.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

namespace PlasmaIntegration {

    namespace {

        constexpr const char* kPluginId = "org.wallpaperanim.video";
        constexpr const char* kDefaultPlugin = "org.kde.image";

        QString userPluginDir()
        {
            const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
            return base + QStringLiteral("/plasma/wallpapers/") + QString::fromUtf8(kPluginId);
        }

        // The packaged plugin lives next to the installed data files; a build tree run
        // finds it relative to the executable instead.
        QString sourcePluginDir()
        {
            // Executable-relative paths come first on purpose. A binary run straight out
            // of the build tree must package the repository's copy; consulting the
            // install prefix first made such a run adopt a previously installed (and by
            // then stale) copy as its own source, so it could never refresh it.
            const QString appDir = QCoreApplication::applicationDirPath();
            QStringList candidates;
            candidates << appDir + QStringLiteral("/../data/plasma/wallpapers/") + QString::fromUtf8(kPluginId)
                       << appDir + QStringLiteral("/../../data/plasma/wallpapers/") + QString::fromUtf8(kPluginId)
                       << appDir + QStringLiteral("/../share/plasma/wallpapers/") + QString::fromUtf8(kPluginId);
#ifdef WPA_INSTALL_DATADIR
            candidates << QString::fromUtf8(WPA_INSTALL_DATADIR) + QStringLiteral("/plasma/wallpapers/")
                            + QString::fromUtf8(kPluginId);
#endif
            candidates << QStringLiteral("/usr/share/plasma/wallpapers/") + QString::fromUtf8(kPluginId)
                       << QStringLiteral("/usr/local/share/plasma/wallpapers/") + QString::fromUtf8(kPluginId);

            for (const QString& candidate : std::as_const(candidates)) {
                if (QFileInfo::exists(candidate + QStringLiteral("/metadata.json"))) {
                    return QDir(candidate).canonicalPath();
                }
            }
            return {};
        }

        bool copyTree(const QString& from, const QString& to, QString* errorOut)
        {
            QDir source(from);
            if (!source.exists()) {
                if (errorOut) *errorOut = QStringLiteral("Missing source directory: %1").arg(from);
                return false;
            }
            if (!QDir().mkpath(to)) {
                if (errorOut) *errorOut = QStringLiteral("Cannot create %1").arg(to);
                return false;
            }

            const QFileInfoList entries =
                source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
            for (const QFileInfo& entry : entries) {
                const QString target = to + QLatin1Char('/') + entry.fileName();
                if (entry.isDir()) {
                    if (!copyTree(entry.absoluteFilePath(), target, errorOut)) return false;
                } else {
                    QFile::remove(target);
                    if (!QFile::copy(entry.absoluteFilePath(), target)) {
                        if (errorOut) *errorOut = QStringLiteral("Cannot copy %1").arg(entry.absoluteFilePath());
                        return false;
                    }
                    QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner
                                                      | QFile::ReadGroup | QFile::ReadOther);
                }
            }
            return true;
        }

        // Escapes a value for embedding in a single-quoted JavaScript literal.
        QString jsQuote(const QString& value)
        {
            QString escaped = value;
            escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
            escaped.replace(QLatin1Char('\''), QStringLiteral("\\'"));
            return escaped;
        }

        // True when any packaged file is missing from, or differs from, the install.
        bool treesDiffer(const QString& from, const QString& to)
        {
            QDir source(from);
            const QFileInfoList entries =
                source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
            for (const QFileInfo& entry : entries) {
                const QString target = to + QLatin1Char('/') + entry.fileName();
                if (entry.isDir()) {
                    if (treesDiffer(entry.absoluteFilePath(), target)) return true;
                    continue;
                }

                QFile sourceFile(entry.absoluteFilePath());
                QFile targetFile(target);
                if (!targetFile.exists()) return true;
                if (sourceFile.size() != targetFile.size()) return true;
                if (!sourceFile.open(QIODevice::ReadOnly) || !targetFile.open(QIODevice::ReadOnly)) {
                    return true;
                }
                if (sourceFile.readAll() != targetFile.readAll()) return true;
            }
            return false;
        }

        // plasmashell's desktops() lists the containments of the *current activity*,
        // which is not the same question as "what is painted on the outputs right now".
        // A session whose current activity has gone empty - kactivitymanagerd never
        // wrote [main]currentActivity, or an activity switch half-failed - keeps
        // returning the activity's containment with screen == -1 while an entirely
        // different, activity-less containment owns screen 0. Setting wallpaperPlugin
        // on that orphan succeeds, reports success, and changes nothing on screen.
        // desktopForScreen() asks the question that matters, so every script below
        // drives off it and only falls back to desktops() when there is no output.
        QString desktopListPrologue()
        {
            return QStringLiteral(
                "var wpaTargets = [];\n"
                "var wpaSeen = {};\n"
                "for (var wpaS = 0; wpaS < screenCount; wpaS++) {\n"
                "    var wpaC = desktopForScreen(wpaS);\n"
                "    if (!wpaC || wpaSeen[wpaC.id]) continue;\n"
                "    wpaSeen[wpaC.id] = true;\n"
                "    wpaTargets.push(wpaC);\n"
                "}\n"
                "if (wpaTargets.length == 0) {\n"
                "    var wpaAll = desktops();\n"
                "    for (var wpaI = 0; wpaI < wpaAll.length; wpaI++) wpaTargets.push(wpaAll[wpaI]);\n"
                "}\n");
        }

        QString evaluateScript(const QString& script)
        {
            QDBusInterface shell(QStringLiteral("org.kde.plasmashell"),
                                 QStringLiteral("/PlasmaShell"),
                                 QStringLiteral("org.kde.PlasmaShell"),
                                 QDBusConnection::sessionBus());
            if (!shell.isValid()) return {};
            QDBusReply<QString> reply = shell.call(QStringLiteral("evaluateScript"), script);
            if (!reply.isValid()) return {};
            return reply.value();
        }

        bool setWallpaperPlugin(const QString& pluginId, QString* errorOut)
        {
            if (!IsPlasmaShellRunning()) {
                if (errorOut) *errorOut = QStringLiteral("plasmashell is not running");
                return false;
            }

            QString script = desktopListPrologue();
            script += QStringLiteral(
                          "for (var i = 0; i < wpaTargets.length; i++) {\n"
                          "    wpaTargets[i].wallpaperPlugin = '%1';\n")
                          .arg(pluginId);

            if (pluginId == QString::fromUtf8(kPluginId)) {
                // Seed the plugin's state in the same call, so it has something to play
                // the moment plasmashell instantiates it.
                const auto& cfg = Config::ConfigManager::GetInstance().GetConfig();
                script += QStringLiteral(
                              "    wpaTargets[i].currentConfigGroup = ['Wallpaper', '%1', 'General'];\n"
                              "    wpaTargets[i].writeConfig('MediaPath', '%2');\n"
                              "    wpaTargets[i].writeConfig('FitMode', %3);\n")
                              .arg(pluginId, jsQuote(QString::fromStdString(cfg.lastVideoPath)))
                              .arg(cfg.fitMode);
            }
            // Report back how many containments were touched: an empty target list is
            // the difference between "the wallpaper changed" and "the call succeeded".
            script += QStringLiteral("}\nprint(wpaTargets.length);\n");

            QDBusInterface shell(QStringLiteral("org.kde.plasmashell"),
                                 QStringLiteral("/PlasmaShell"),
                                 QStringLiteral("org.kde.PlasmaShell"),
                                 QDBusConnection::sessionBus());
            QDBusReply<QString> reply = shell.call(QStringLiteral("evaluateScript"), script);
            if (!reply.isValid()) {
                if (errorOut) *errorOut = reply.error().message();
                return false;
            }
            if (reply.value().trimmed() == QLatin1String("0")) {
                if (errorOut) *errorOut = QStringLiteral("plasmashell reports no desktop containment");
                return false;
            }
            return true;
        }

        // plasmashell persists the active plugin here; reading it is the fallback for
        // when the shell is unreachable. It cannot tell which containment is on screen,
        // so it answers the stricter question - do *all* desktop containments use this
        // plugin? A session carrying a stale extra containment (an old activity, a
        // monitor that is gone) then correctly reads as "not fully applied".
        // Counts the desktop containments in the persisted layout, and how many of them
        // carry pluginId. Returns false when the file cannot be read at all.
        bool countContainments(const QString& pluginId, int* totalOut, int* matchingOut)
        {
            const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                                 + QStringLiteral("/plasma-org.kde.plasma.desktop-appletsrc");
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};

            QTextStream in(&file);
            bool inContainment = false;
            QString wallpaperPlugin;
            QString containmentPlugin;
            int desktopContainments = 0;
            int matching = 0;

            // Desktop containments are the folder view / plain desktop ones; panels
            // carry a wallpaperplugin key too and must not be mistaken for them.
            const auto flush = [&]() {
                if (containmentPlugin.isEmpty() || containmentPlugin == QLatin1String("org.kde.panel")) {
                    return;
                }
                ++desktopContainments;
                if (wallpaperPlugin == pluginId) ++matching;
            };

            while (!in.atEnd()) {
                const QString line = in.readLine().trimmed();
                if (line.startsWith(QLatin1Char('['))) {
                    // A new group: only top-level [Containments][N] groups matter.
                    static const QRegularExpression containmentGroup(
                        QStringLiteral("^\\[Containments\\]\\[\\d+\\]$"));
                    if (inContainment) flush();
                    inContainment = containmentGroup.match(line).hasMatch();
                    wallpaperPlugin.clear();
                    containmentPlugin.clear();
                    continue;
                }
                if (!inContainment) continue;

                if (line.startsWith(QLatin1String("plugin="))) {
                    containmentPlugin = line.mid(7);
                } else if (line.startsWith(QLatin1String("wallpaperplugin="))) {
                    wallpaperPlugin = line.mid(16);
                }
            }
            if (inContainment) flush();

            if (totalOut) *totalOut = desktopContainments;
            if (matchingOut) *matchingOut = matching;
            return true;
        }

        bool everyContainmentUses(const QString& pluginId)
        {
            int total = 0;
            int matching = 0;
            if (!countContainments(pluginId, &total, &matching)) return false;
            return total > 0 && total == matching;
        }

    } // namespace

    QString PluginId() { return QString::fromUtf8(kPluginId); }

    bool IsPlasmaSession()
    {
        const QString desktops =
            QProcessEnvironment::systemEnvironment().value(QStringLiteral("XDG_CURRENT_DESKTOP"));
        return desktops.contains(QStringLiteral("KDE"), Qt::CaseInsensitive)
               || desktops.contains(QStringLiteral("plasma"), Qt::CaseInsensitive);
    }

    bool IsPlasmaShellRunning()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) return false;
        QDBusReply<bool> reply = bus.interface()->isServiceRegistered(QStringLiteral("org.kde.plasmashell"));
        return reply.isValid() && reply.value();
    }

    bool InstallPlugin(QString* errorOut, bool* updatedOut)
    {
        if (updatedOut) *updatedOut = false;

        const QString source = sourcePluginDir();
        if (source.isEmpty()) {
            if (errorOut) *errorOut = QStringLiteral("Plasma wallpaper plugin package not found");
            return false;
        }

        const QString target = userPluginDir();
        // Running from the installed copy: source and target are the same files.
        if (QDir(target).canonicalPath() == source) return true;

        if (!treesDiffer(source, target)) return true;
        if (!copyTree(source, target, errorOut)) return false;

        if (updatedOut) *updatedOut = true;
        return true;
    }

    bool RestartPlasmaShell()
    {
        // plasmashell's QML engine caches a component per URL for the life of the
        // process, so replacing the package's files under a running shell keeps the old
        // wallpaper code loaded. A restart is the only way to pick the new one up.
        if (QProcess::startDetached(QStringLiteral("systemctl"),
                                    {QStringLiteral("--user"), QStringLiteral("restart"),
                                     QStringLiteral("plasma-plasmashell")})) {
            return true;
        }
        return QProcess::startDetached(QStringLiteral("kstart"), {QStringLiteral("plasmashell")});
    }

    bool IsActive()
    {
        // Ask plasmashell directly: evaluateScript hands back whatever the script
        // prints, which is authoritative even before the config file is flushed.
        if (IsPlasmaShellRunning()) {
            const QString output = evaluateScript(desktopListPrologue()
                                                  + QStringLiteral(
                                                      "var plugins = [];\n"
                                                      "for (var i = 0; i < wpaTargets.length; i++) {\n"
                                                      "    plugins.push(wpaTargets[i].wallpaperPlugin);\n"
                                                      "}\n"
                                                      "print(plugins.join(','));\n"))
                                       .trimmed();
            if (!output.isEmpty()) {
                // Every screen must be ours. Answering "one of them is" let a session
                // where plasmashell had moved the desktop to another containment read
                // as active for ever, so nothing ever re-took the desktop.
                const QStringList plugins = output.split(QLatin1Char(','));
                for (const QString& plugin : plugins) {
                    if (plugin.trimmed() != QString::fromUtf8(kPluginId)) return false;
                }
                return true;
            }
        }
        // plasmashell unreachable: fall back to what it last persisted.
        return everyContainmentUses(QString::fromUtf8(kPluginId));
    }

    bool IsConfiguredAnywhere()
    {
        int total = 0;
        int matching = 0;
        if (!countContainments(QString::fromUtf8(kPluginId), &total, &matching)) return false;
        return matching > 0;
    }

    bool Activate(QString* errorOut)
    {
        bool packageUpdated = false;
        if (!InstallPlugin(errorOut, &packageUpdated)) return false;

        // Bounce through the stock plugin so plasmashell instantiates the wallpaper
        // afresh rather than keeping the previous item alive.
        if (IsActive()) {
            setWallpaperPlugin(QString::fromUtf8(kDefaultPlugin), nullptr);
        }
        if (!setWallpaperPlugin(QString::fromUtf8(kPluginId), errorOut)) return false;

        // The config write above is flushed when plasmashell shuts down, so the new
        // instance comes back up with the wallpaper already selected.
        if (packageUpdated) RestartPlasmaShell();
        return true;
    }

    bool Restore(QString* errorOut)
    {
        return setWallpaperPlugin(QString::fromUtf8(kDefaultPlugin), errorOut);
    }

    bool PushState(const QString& mediaPath, int fitMode, bool paused, QString* errorOut)
    {
        if (!IsPlasmaShellRunning()) {
            if (errorOut) *errorOut = QStringLiteral("plasmashell is not running");
            return false;
        }

        const QString script = desktopListPrologue()
                               + QStringLiteral(
                                   "for (var i = 0; i < wpaTargets.length; i++) {\n"
                                   "    if (wpaTargets[i].wallpaperPlugin != '%1') {\n"
                                   "        continue;\n"
                                   "    }\n"
                                   "    wpaTargets[i].currentConfigGroup = ['Wallpaper', '%1', 'General'];\n"
                                   "    wpaTargets[i].writeConfig('MediaPath', '%2');\n"
                                   "    wpaTargets[i].writeConfig('FitMode', %3);\n"
                                   "    wpaTargets[i].writeConfig('Paused', %4);\n"
                                   "    wpaTargets[i].reloadConfig();\n"
                                   "}\n")
                                     .arg(QString::fromUtf8(kPluginId), jsQuote(mediaPath))
                                     .arg(fitMode)
                                     .arg(paused ? QStringLiteral("true") : QStringLiteral("false"));

        QDBusInterface shell(QStringLiteral("org.kde.plasmashell"),
                             QStringLiteral("/PlasmaShell"),
                             QStringLiteral("org.kde.PlasmaShell"),
                             QDBusConnection::sessionBus());
        QDBusReply<QString> reply = shell.call(QStringLiteral("evaluateScript"), script);
        if (!reply.isValid()) {
            if (errorOut) *errorOut = reply.error().message();
            return false;
        }
        return true;
    }

}
