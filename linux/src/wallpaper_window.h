#pragma once

#include <QElapsedTimer>
#include <QOpenGLWindow>
#include <QString>

#include <mpv/client.h>
#include <mpv/render_gl.h>

class QTimer;
class ShaderRenderer;

// One wallpaper surface, sized to a single screen.
//
// The Windows build parents a WS_POPUP window into WorkerW so it lands behind the
// desktop icons. The Wayland equivalent is a wlr-layer-shell surface on the
// background layer; video frames come from libmpv's render API (which keeps decoded
// frames on the GPU), and GLSL shaders are drawn by ShaderRenderer instead.
class WallpaperWindow : public QOpenGLWindow {
    Q_OBJECT

public:
    explicit WallpaperWindow(QScreen* targetScreen, int layerChoice);
    ~WallpaperWindow() override;

    // Applies the media at filePath (video, GIF or GLSL shader). Safe to call before
    // the GL context exists; the load is replayed once the window is initialized.
    bool setMedia(const QString& filePath, QString* errorOut = nullptr);

    void setPaused(bool paused);
    bool isPaused() const { return m_paused; }

    void setFitMode(int fitMode);
    void setMaxFps(int maxFps);
    void setHardwareDecode(bool enabled);
    void setVolume(int volume);

    // Milliseconds since the compositor last let us draw. Used to notice that the
    // wallpaper is completely covered so decoding can stop.
    qint64 msSinceLastPaint() const;

Q_SIGNALS:
    void mediaFailed(const QString& message);

protected:
    void initializeGL() override;
    void paintGL() override;

private Q_SLOTS:
    void onMpvRedraw();
    void onMpvEvents();
    void onShaderTick();

private:
    bool createMpv();
    void destroyMpv();
    void applyFitMode();
    void applyFpsLimit();
    bool loadShader(const QString& filePath, QString* errorOut);
    void unloadShader();
    void setMpvOption(const char* name, const QString& value);
    void setMpvProperty(const char* name, const QString& value);

    mpv_handle* m_mpv = nullptr;
    mpv_render_context* m_renderCtx = nullptr;

    ShaderRenderer* m_shader = nullptr;
    QTimer* m_shaderTimer = nullptr;
    bool m_shaderMode = false;

    QString m_pendingPath;
    QString m_currentPath;
    bool m_glReady = false;
    bool m_paused = false;

    int m_fitMode = 0;
    int m_maxFps = 0;
    bool m_hardwareDecode = true;
    int m_volume = 0;
    int m_layerChoice = 0;

    QElapsedTimer m_paintClock;
    qint64 m_lastPaintMs = 0;
};
