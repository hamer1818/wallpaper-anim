#include "autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

namespace Autostart {

    namespace {
        QString entryPath()
        {
            const QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
            return base + QStringLiteral("/autostart/wallpaperanim.desktop");
        }
    }

    bool IsEnabled() { return QFileInfo::exists(entryPath()); }

    bool SetEnabled(bool enabled, QString* errorOut)
    {
        const QString path = entryPath();

        if (!enabled) {
            if (!QFileInfo::exists(path)) return true;
            if (!QFile::remove(path)) {
                if (errorOut) *errorOut = QStringLiteral("Cannot remove %1").arg(path);
                return false;
            }
            return true;
        }

        if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
            if (errorOut) *errorOut = QStringLiteral("Cannot create the autostart directory");
            return false;
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            if (errorOut) *errorOut = QStringLiteral("Cannot write %1").arg(path);
            return false;
        }

        QTextStream out(&file);
        out << "[Desktop Entry]\n"
            << "Type=Application\n"
            << "Name=WallpaperAnim\n"
            << "Comment=Animated wallpaper engine\n"
            << "Exec=" << QCoreApplication::applicationFilePath() << " --background\n"
            << "Icon=wallpaperanim\n"
            << "Terminal=false\n"
            << "X-GNOME-Autostart-enabled=true\n";
        file.close();
        return true;
    }

}
