#include "youtube.h"

#include "config.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

YoutubeDownloader::YoutubeDownloader(QObject* parent) : QObject(parent) {}

YoutubeDownloader::~YoutubeDownloader()
{
    Cancel();
}

QString YoutubeDownloader::ExecutablePath()
{
    return QStandardPaths::findExecutable(QStringLiteral("yt-dlp"));
}

bool YoutubeDownloader::IsAvailable()
{
    return !ExecutablePath().isEmpty();
}

bool YoutubeDownloader::IsRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void YoutubeDownloader::Cancel()
{
    if (!m_process) return;
    m_process->disconnect(this);
    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) m_process->kill();
    }
    m_process->deleteLater();
    m_process = nullptr;
}

void YoutubeDownloader::Start(const QString& url, int maxHeight)
{
    if (IsRunning()) {
        Q_EMIT failed(QStringLiteral("A download is already running"));
        return;
    }

    const QString ytDlp = ExecutablePath();
    if (ytDlp.isEmpty()) {
        Q_EMIT failed(QStringLiteral("yt-dlp not found"));
        return;
    }

    m_url = url.trimmed();
    m_maxHeight = maxHeight > 0 ? maxHeight : 1080;
    m_videoId.clear();
    m_title.clear();
    m_probing = true;

    // First pass: resolve id and title without downloading anything.
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        if (!m_probing) return;
        m_probing = false;

        const QString output = QString::fromUtf8(m_process->readAllStandardOutput()).trimmed();
        const QString errors = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        m_process->deleteLater();
        m_process = nullptr;

        if (status != QProcess::NormalExit || exitCode != 0 || output.isEmpty()) {
            Q_EMIT failed(errors.isEmpty() ? QStringLiteral("Could not read video info") : errors);
            return;
        }

        const QStringList parts = output.split(QLatin1Char('|'));
        m_videoId = parts.value(0).trimmed();
        m_title = parts.value(1).trimmed();
        if (m_videoId.isEmpty()) {
            Q_EMIT failed(QStringLiteral("Could not read video id"));
            return;
        }
        if (m_title.isEmpty()) m_title = m_videoId;

        startDownload();
    });

    Q_EMIT progress(0, QStringLiteral("info"));
    m_process->start(ytDlp, {QStringLiteral("--no-warnings"),
                             QStringLiteral("--skip-download"),
                             QStringLiteral("--no-playlist"),
                             QStringLiteral("--print"), QStringLiteral("%(id)s|%(title)s"),
                             m_url});
}

void YoutubeDownloader::startDownload()
{
    const QString ytDlp = ExecutablePath();
    const QString outputDir = QString::fromStdString(Config::DownloadsDir());
    const QString outputTemplate = outputDir + QLatin1Char('/') + m_videoId + QStringLiteral(".%(ext)s");

    const QString format =
        QStringLiteral("bv*[height<=%1]+ba/b[height<=%1]/bv*+ba/b").arg(m_maxHeight);

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        static const QRegularExpression percentPattern(QStringLiteral("\\[download\\]\\s+(\\d+(?:\\.\\d+)?)%"));
        const QString chunk = QString::fromUtf8(m_process->readAllStandardOutput());
        const QStringList lines = chunk.split(QRegularExpression(QStringLiteral("[\\r\\n]")),
                                              Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            const auto match = percentPattern.match(line);
            if (match.hasMatch()) {
                Q_EMIT progress(static_cast<int>(match.captured(1).toDouble()), line.trimmed());
            }
        }
    });

    connect(m_process, &QProcess::finished, this, &YoutubeDownloader::handleDownloadFinished);

    m_process->start(ytDlp, {QStringLiteral("--no-warnings"),
                             QStringLiteral("--no-playlist"),
                             QStringLiteral("--newline"),
                             QStringLiteral("--no-part"),
                             QStringLiteral("-f"), format,
                             QStringLiteral("--merge-output-format"), QStringLiteral("mp4"),
                             QStringLiteral("-o"), outputTemplate,
                             m_url});
}

void YoutubeDownloader::handleDownloadFinished(int exitCode, QProcess::ExitStatus status)
{
    const QString output = m_process ? QString::fromUtf8(m_process->readAllStandardOutput()) : QString();
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }

    if (status != QProcess::NormalExit || exitCode != 0) {
        Q_EMIT failed(output.trimmed().isEmpty() ? QStringLiteral("yt-dlp failed") : output.trimmed());
        return;
    }

    // yt-dlp picked the container, so find whatever "<id>.<ext>" it produced.
    QDir downloads(QString::fromStdString(Config::DownloadsDir()));
    const QStringList matches = downloads.entryList({m_videoId + QStringLiteral(".*")}, QDir::Files,
                                                    QDir::Time);
    if (matches.isEmpty()) {
        Q_EMIT failed(QStringLiteral("Downloaded file not found"));
        return;
    }

    Q_EMIT progress(100, QStringLiteral("done"));
    Q_EMIT finished(downloads.absoluteFilePath(matches.first()), m_title);
}
