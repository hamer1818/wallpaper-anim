#include "thumbnail.h"

#include "config.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace Thumbnail {

    bool IsFfmpegAvailable()
    {
        return !QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty();
    }

    QString Generate(const QString& mediaPath)
    {
        const QFileInfo info(mediaPath);
        if (!info.exists()) return {};
        if (Config::IsShaderPath(mediaPath.toStdString())) return {};
        if (!IsFfmpegAvailable()) return {};

        const QByteArray key = QCryptographicHash::hash(mediaPath.toUtf8(), QCryptographicHash::Sha1);
        const QString target = QString::fromStdString(Config::ThumbsDir()) + QLatin1Char('/')
                               + QString::fromLatin1(key.toHex()) + QStringLiteral(".jpg");

        if (QFileInfo::exists(target)) return target;

        // GIFs and very short clips have no frame at 1s, so seek only for real video.
        const bool isGif = mediaPath.endsWith(QStringLiteral(".gif"), Qt::CaseInsensitive);
        QStringList args;
        args << QStringLiteral("-y") << QStringLiteral("-loglevel") << QStringLiteral("error");
        if (!isGif) args << QStringLiteral("-ss") << QStringLiteral("1");
        args << QStringLiteral("-i") << mediaPath
             << QStringLiteral("-frames:v") << QStringLiteral("1")
             << QStringLiteral("-vf") << QStringLiteral("scale=480:-2")
             << QStringLiteral("-q:v") << QStringLiteral("4")
             << target;

        QProcess ffmpeg;
        ffmpeg.start(QStringLiteral("ffmpeg"), args);
        if (!ffmpeg.waitForFinished(20000)) {
            ffmpeg.kill();
            ffmpeg.waitForFinished(2000);
            return {};
        }

        if (ffmpeg.exitStatus() != QProcess::NormalExit || ffmpeg.exitCode() != 0) {
            // Retry from the very first frame: the seek target may be past the end.
            if (!isGif) {
                QStringList retry;
                retry << QStringLiteral("-y") << QStringLiteral("-loglevel") << QStringLiteral("error")
                      << QStringLiteral("-i") << mediaPath
                      << QStringLiteral("-frames:v") << QStringLiteral("1")
                      << QStringLiteral("-vf") << QStringLiteral("scale=480:-2")
                      << QStringLiteral("-q:v") << QStringLiteral("4")
                      << target;
                QProcess retryProcess;
                retryProcess.start(QStringLiteral("ffmpeg"), retry);
                if (retryProcess.waitForFinished(20000) && retryProcess.exitCode() == 0
                    && QFileInfo::exists(target)) {
                    return target;
                }
            }
            return {};
        }

        return QFileInfo::exists(target) ? target : QString();
    }

}
