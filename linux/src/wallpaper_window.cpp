#include "wallpaper_window.h"

#include "config.h"
#include "shader_renderer.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QScreen>
#include <QTimer>

#ifdef WPA_HAVE_LAYER_SHELL
#include <LayerShellQt/Window>
#endif

namespace {

void* mpvGetProcAddress(void* ctx, const char* name)
{
    Q_UNUSED(ctx);
    QOpenGLContext* glCtx = QOpenGLContext::currentContext();
    if (!glCtx) return nullptr;
    return reinterpret_cast<void*>(glCtx->getProcAddress(QByteArray(name)));
}

void onMpvRenderUpdate(void* ctx)
{
    // Called from mpv's render thread - bounce to the GUI thread before touching Qt.
    QMetaObject::invokeMethod(static_cast<WallpaperWindow*>(ctx), "onMpvRedraw", Qt::QueuedConnection);
}

void onMpvWakeup(void* ctx)
{
    QMetaObject::invokeMethod(static_cast<WallpaperWindow*>(ctx), "onMpvEvents", Qt::QueuedConnection);
}

} // namespace

WallpaperWindow::WallpaperWindow(QScreen* targetScreen, int layerChoice)
    : m_layerChoice(layerChoice)
{
    setTitle(QStringLiteral("WallpaperAnim"));
    setFlags(flags() | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus
             | Qt::WindowStaysOnBottomHint);
    if (targetScreen) setScreen(targetScreen);

#ifdef WPA_HAVE_LAYER_SHELL
    // Must be configured before the platform surface is created.
    auto* layerWindow = LayerShellQt::Window::get(this);
    layerWindow->setLayer(m_layerChoice == Config::LayerBottom ? LayerShellQt::Window::LayerBottom
                                                              : LayerShellQt::Window::LayerBackground);
    layerWindow->setAnchors({LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                             | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight});
    // -1 keeps panels from reserving space away from the wallpaper.
    layerWindow->setExclusiveZone(-1);
    layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    layerWindow->setScope(QStringLiteral("wallpaper"));
    layerWindow->setCloseOnDismissed(false);
    layerWindow->setActivateOnShow(false);
    if (targetScreen) layerWindow->setScreen(targetScreen);
#endif

    m_paintClock.start();
    m_lastPaintMs = m_paintClock.elapsed();

    m_shaderTimer = new QTimer(this);
    connect(m_shaderTimer, &QTimer::timeout, this, &WallpaperWindow::onShaderTick);
}

WallpaperWindow::~WallpaperWindow()
{
    // Both the mpv render context and the shader program own GL objects, so the
    // context has to be current while they are torn down.
    if (m_glReady && context()) {
        makeCurrent();
        if (m_renderCtx) {
            mpv_render_context_free(m_renderCtx);
            m_renderCtx = nullptr;
        }
        if (m_shader) {
            m_shader->cleanup();
            delete m_shader;
            m_shader = nullptr;
        }
        doneCurrent();
    } else {
        delete m_shader;
        m_shader = nullptr;
    }
    destroyMpv();
}

void WallpaperWindow::destroyMpv()
{
    if (m_renderCtx) {
        mpv_render_context_free(m_renderCtx);
        m_renderCtx = nullptr;
    }
    if (m_mpv) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

void WallpaperWindow::setMpvOption(const char* name, const QString& value)
{
    if (!m_mpv) return;
    mpv_set_option_string(m_mpv, name, value.toUtf8().constData());
}

void WallpaperWindow::setMpvProperty(const char* name, const QString& value)
{
    if (!m_mpv) return;
    mpv_set_property_string(m_mpv, name, value.toUtf8().constData());
}

bool WallpaperWindow::createMpv()
{
    if (m_mpv) return true;

    m_mpv = mpv_create();
    if (!m_mpv) return false;

    // Wallpaper playback: no UI, no input, silent by default, loops forever.
    setMpvOption("vo", QStringLiteral("libmpv"));
    setMpvOption("idle", QStringLiteral("yes"));
    setMpvOption("loop-file", QStringLiteral("inf"));
    setMpvOption("keep-open", QStringLiteral("always"));
    setMpvOption("osc", QStringLiteral("no"));
    setMpvOption("osd-level", QStringLiteral("0"));
    setMpvOption("input-default-bindings", QStringLiteral("no"));
    setMpvOption("input-vo-keyboard", QStringLiteral("no"));
    setMpvOption("input-cursor", QStringLiteral("no"));
    setMpvOption("cursor-autohide", QStringLiteral("no"));
    setMpvOption("terminal", QStringLiteral("no"));
    setMpvOption("config", QStringLiteral("no"));
    setMpvOption("ytdl", QStringLiteral("no"));
    setMpvOption("hwdec", m_hardwareDecode ? QStringLiteral("auto-safe") : QStringLiteral("no"));
    setMpvOption("mute", m_volume > 0 ? QStringLiteral("no") : QStringLiteral("yes"));
    setMpvOption("volume", QString::number(qBound(0, m_volume, 100)));

    if (mpv_initialize(m_mpv) < 0) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return false;
    }

    mpv_set_wakeup_callback(m_mpv, onMpvWakeup, this);
    applyFitMode();
    applyFpsLimit();
    return true;
}

void WallpaperWindow::initializeGL()
{
    m_glReady = true;

    if (!createMpv()) {
        Q_EMIT mediaFailed(QStringLiteral("libmpv could not be initialized"));
        return;
    }

    mpv_opengl_init_params glInit{mpvGetProcAddress, this};
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    if (mpv_render_context_create(&m_renderCtx, m_mpv, params) < 0) {
        m_renderCtx = nullptr;
        Q_EMIT mediaFailed(QStringLiteral("libmpv OpenGL render context could not be created"));
        return;
    }
    mpv_render_context_set_update_callback(m_renderCtx, onMpvRenderUpdate, this);

    if (!m_pendingPath.isEmpty()) {
        const QString path = m_pendingPath;
        m_pendingPath.clear();
        setMedia(path);
    }
}

bool WallpaperWindow::setMedia(const QString& filePath, QString* errorOut)
{
    if (filePath.isEmpty()) return false;

    if (!QFileInfo::exists(filePath)) {
        if (errorOut) *errorOut = QStringLiteral("File not found: %1").arg(filePath);
        return false;
    }

    if (!m_glReady) {
        // Replayed from initializeGL() once the surface is up.
        m_pendingPath = filePath;
        m_currentPath = filePath;
        return true;
    }

    m_currentPath = filePath;

    const std::string utf8Path = filePath.toStdString();
    if (Config::IsShaderPath(utf8Path)) {
        if (m_mpv) mpv_command_string(m_mpv, "stop");
        return loadShader(filePath, errorOut);
    }

    unloadShader();

    if (!m_mpv && !createMpv()) {
        if (errorOut) *errorOut = QStringLiteral("libmpv could not be initialized");
        return false;
    }

    const QByteArray pathUtf8 = filePath.toUtf8();
    const char* cmd[] = {"loadfile", pathUtf8.constData(), "replace", nullptr};
    if (mpv_command(m_mpv, cmd) < 0) {
        if (errorOut) *errorOut = QStringLiteral("mpv rejected the file: %1").arg(filePath);
        return false;
    }

    applyFitMode();
    applyFpsLimit();
    setMpvProperty("pause", m_paused ? QStringLiteral("yes") : QStringLiteral("no"));
    return true;
}

bool WallpaperWindow::loadShader(const QString& filePath, QString* errorOut)
{
    if (filePath.endsWith(QStringLiteral(".hlsl"), Qt::CaseInsensitive)) {
        if (errorOut) *errorOut = QStringLiteral("HLSL shaders are Windows-only; use .glsl/.frag");
        return false;
    }

    if (!m_glReady || !context()) {
        m_pendingPath = filePath;
        return true;
    }

    makeCurrent();
    if (!m_shader) m_shader = new ShaderRenderer();

    QString compileError;
    const bool ok = m_shader->load(filePath, &compileError);
    doneCurrent();

    if (!ok) {
        if (errorOut) *errorOut = compileError;
        m_shaderMode = false;
        m_shaderTimer->stop();
        return false;
    }

    m_shaderMode = true;
    const int fps = m_maxFps > 0 ? m_maxFps : 60;
    m_shaderTimer->start(qMax(1, 1000 / fps));
    update();
    return true;
}

void WallpaperWindow::unloadShader()
{
    if (!m_shaderMode) return;
    m_shaderMode = false;
    m_shaderTimer->stop();
    if (m_shader && m_glReady && context()) {
        makeCurrent();
        m_shader->cleanup();
        doneCurrent();
    }
}

void WallpaperWindow::onShaderTick()
{
    if (m_shaderMode && !m_paused) update();
}

void WallpaperWindow::onMpvRedraw()
{
    if (!m_renderCtx) return;
    const uint64_t flags = mpv_render_context_update(m_renderCtx);
    if (flags & MPV_RENDER_UPDATE_FRAME) update();
}

void WallpaperWindow::onMpvEvents()
{
    if (!m_mpv) return;
    while (m_mpv) {
        mpv_event* event = mpv_wait_event(m_mpv, 0);
        if (!event || event->event_id == MPV_EVENT_NONE) break;

        switch (event->event_id) {
        case MPV_EVENT_END_FILE: {
            auto* endFile = static_cast<mpv_event_end_file*>(event->data);
            if (endFile && endFile->reason == MPV_END_FILE_REASON_ERROR) {
                Q_EMIT mediaFailed(QString::fromUtf8(mpv_error_string(endFile->error)));
            }
            break;
        }
        case MPV_EVENT_FILE_LOADED:
            applyFitMode();
            break;
        case MPV_EVENT_SHUTDOWN:
            return;
        default:
            break;
        }
    }
}

void WallpaperWindow::paintGL()
{
    m_lastPaintMs = m_paintClock.elapsed();

    const qreal dpr = devicePixelRatio();
    const int widthPx = qMax(1, static_cast<int>(width() * dpr));
    const int heightPx = qMax(1, static_cast<int>(height() * dpr));

    if (m_shaderMode && m_shader && m_shader->isValid()) {
        m_shader->render(widthPx, heightPx);
        return;
    }

    if (!m_renderCtx) {
        if (QOpenGLContext* ctx = QOpenGLContext::currentContext()) {
            QOpenGLFunctions* f = ctx->functions();
            f->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            f->glClear(GL_COLOR_BUFFER_BIT);
        }
        return;
    }

    mpv_opengl_fbo fbo{static_cast<int>(defaultFramebufferObject()), widthPx, heightPx, 0};
    int flipY = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flipY},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    mpv_render_context_render(m_renderCtx, params);
}

void WallpaperWindow::setPaused(bool paused)
{
    if (m_paused == paused) return;
    m_paused = paused;

    if (m_shaderMode) {
        if (paused) {
            m_shaderTimer->stop();
        } else {
            const int fps = m_maxFps > 0 ? m_maxFps : 60;
            m_shaderTimer->start(qMax(1, 1000 / fps));
        }
        return;
    }

    setMpvProperty("pause", paused ? QStringLiteral("yes") : QStringLiteral("no"));
}

void WallpaperWindow::setFitMode(int fitMode)
{
    if (m_fitMode == fitMode) return;
    m_fitMode = fitMode;
    applyFitMode();
}

void WallpaperWindow::applyFitMode()
{
    if (!m_mpv) return;

    switch (m_fitMode) {
    case 1: // Fit: whole frame visible, black bars where the aspect differs
        setMpvProperty("keepaspect", QStringLiteral("yes"));
        setMpvProperty("panscan", QStringLiteral("0.0"));
        setMpvProperty("video-unscaled", QStringLiteral("no"));
        break;
    case 2: // Stretch: fill exactly, ignore aspect ratio
        setMpvProperty("keepaspect", QStringLiteral("no"));
        setMpvProperty("panscan", QStringLiteral("0.0"));
        setMpvProperty("video-unscaled", QStringLiteral("no"));
        break;
    case 3: // Center: native pixels, centered
        setMpvProperty("keepaspect", QStringLiteral("yes"));
        setMpvProperty("panscan", QStringLiteral("0.0"));
        setMpvProperty("video-unscaled", QStringLiteral("yes"));
        break;
    case 0: // Fill: preserve aspect, crop the overflow
    default:
        setMpvProperty("keepaspect", QStringLiteral("yes"));
        setMpvProperty("panscan", QStringLiteral("1.0"));
        setMpvProperty("video-unscaled", QStringLiteral("no"));
        break;
    }
}

void WallpaperWindow::setMaxFps(int maxFps)
{
    if (m_maxFps == maxFps) return;
    m_maxFps = maxFps;
    applyFpsLimit();

    if (m_shaderMode && !m_paused) {
        const int fps = m_maxFps > 0 ? m_maxFps : 60;
        m_shaderTimer->start(qMax(1, 1000 / fps));
    }
}

void WallpaperWindow::applyFpsLimit()
{
    if (!m_mpv) return;
    // 0 means "follow the source", which is what an empty filter chain does.
    if (m_maxFps > 0) {
        setMpvProperty("vf", QStringLiteral("fps=%1").arg(m_maxFps));
    } else {
        setMpvProperty("vf", QString());
    }
}

void WallpaperWindow::setHardwareDecode(bool enabled)
{
    if (m_hardwareDecode == enabled) return;
    m_hardwareDecode = enabled;
    setMpvProperty("hwdec", enabled ? QStringLiteral("auto-safe") : QStringLiteral("no"));
}

void WallpaperWindow::setVolume(int volume)
{
    m_volume = qBound(0, volume, 100);
    setMpvProperty("mute", m_volume > 0 ? QStringLiteral("no") : QStringLiteral("yes"));
    setMpvProperty("volume", QString::number(m_volume));
}

qint64 WallpaperWindow::msSinceLastPaint() const
{
    return m_paintClock.elapsed() - m_lastPaintMs;
}
