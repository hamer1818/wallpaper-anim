#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

// yt-dlp wrapper.
//
// The Windows build deliberately pins H.264 <=1080p because Media Foundation cannot
// decode YouTube's VP9/AV1 on stock Windows 10. mpv/ffmpeg on Linux decode all of
// them, so here the cap is simply the height of the largest screen.
class YoutubeDownloader : public QObject {
    Q_OBJECT

public:
    explicit YoutubeDownloader(QObject* parent = nullptr);
    ~YoutubeDownloader() override;

    static bool IsAvailable();
    static QString ExecutablePath();

    // Downloads url into the app's downloads directory, capped at maxHeight pixels.
    void Start(const QString& url, int maxHeight);
    void Cancel();
    bool IsRunning() const;

Q_SIGNALS:
    void progress(int percent, const QString& statusLine);
    void finished(const QString& filePath, const QString& title);
    void failed(const QString& message);

private:
    void startDownload();
    void handleDownloadFinished(int exitCode, QProcess::ExitStatus status);

    QProcess* m_process = nullptr;
    QString m_url;
    QString m_videoId;
    QString m_title;
    int m_maxHeight = 1080;
    bool m_probing = false;
};
