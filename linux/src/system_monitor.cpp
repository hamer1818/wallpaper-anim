#include "system_monitor.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace SystemMonitor {

    bool IsOnBattery()
    {
        QDir supplies(QStringLiteral("/sys/class/power_supply"));
        if (!supplies.exists()) return false;

        bool sawMains = false;
        const QStringList entries = supplies.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& entry : entries) {
            const QString base = supplies.absoluteFilePath(entry);

            QFile typeFile(base + QStringLiteral("/type"));
            if (!typeFile.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            const QString type = QString::fromUtf8(typeFile.readAll()).trimmed();
            typeFile.close();
            if (type != QLatin1String("Mains")) continue;

            QFile onlineFile(base + QStringLiteral("/online"));
            if (!onlineFile.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            const QString online = QString::fromUtf8(onlineFile.readAll()).trimmed();
            onlineFile.close();

            sawMains = true;
            if (online == QLatin1String("1")) return false; // At least one AC adapter is plugged in.
        }

        // No AC adapter at all means a desktop machine: never "on battery".
        return sawMains;
    }

    bool IsFullscreenAppActive()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) return false;

        {
            QDBusInterface inhibit(QStringLiteral("org.freedesktop.PowerManagement.Inhibit"),
                                   QStringLiteral("/org/freedesktop/PowerManagement/Inhibit"),
                                   QStringLiteral("org.freedesktop.PowerManagement.Inhibit"),
                                   bus);
            if (inhibit.isValid()) {
                QDBusReply<bool> reply = inhibit.call(QStringLiteral("HasInhibit"));
                if (reply.isValid()) return reply.value();
            }
        }

        {
            QDBusInterface screenSaver(QStringLiteral("org.freedesktop.ScreenSaver"),
                                       QStringLiteral("/org/freedesktop/ScreenSaver"),
                                       QStringLiteral("org.freedesktop.ScreenSaver"),
                                       bus);
            if (screenSaver.isValid()) {
                QDBusReply<bool> reply = screenSaver.call(QStringLiteral("GetActive"));
                if (reply.isValid()) return reply.value();
            }
        }

        return false;
    }

}
