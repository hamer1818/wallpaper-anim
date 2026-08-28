#include "shader_renderer.h"

#include <QDateTime>
#include <QFile>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QRegularExpression>
#include <QTextStream>

namespace {

// A single oversized triangle covers the viewport without needing a vertex buffer.
constexpr const char* kVertexShader = R"(#version 330 core
void main()
{
    vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
)";

constexpr const char* kShadertoyPrelude = R"(#version 330 core
out vec4 wpa_FragColor;
uniform vec3 iResolution;
uniform float iTime;
uniform float iTimeDelta;
uniform float iFrameRate;
uniform int iFrame;
uniform vec4 iMouse;
uniform vec4 iDate;
#define iGlobalTime iTime
)";

constexpr const char* kShadertoyEpilogue = R"(
void main()
{
    vec4 color = vec4(0.0, 0.0, 0.0, 1.0);
    mainImage(color, gl_FragCoord.xy);
    wpa_FragColor = color;
}
)";

QString stripVersionDirective(const QString& source)
{
    static const QRegularExpression versionLine(QStringLiteral("^\\s*#version[^\\n]*\\n?"),
                                                QRegularExpression::MultilineOption);
    QString out = source;
    out.remove(versionLine);
    return out;
}

} // namespace

ShaderRenderer::ShaderRenderer() = default;

ShaderRenderer::~ShaderRenderer()
{
    // GL objects must already be gone; cleanup() needs a current context and cannot
    // be called safely from here.
    delete m_program;
    delete m_vao;
}

void ShaderRenderer::resetClock()
{
    m_clock.restart();
    m_frame = 0;
    m_lastNs = 0;
}

bool ShaderRenderer::load(const QString& filePath, QString* errorOut)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut) *errorOut = QStringLiteral("Cannot open shader file: %1").arg(filePath);
        return false;
    }
    QTextStream in(&file);
    const QString source = in.readAll();
    file.close();

    if (!m_initialized) {
        initializeOpenGLFunctions();
        m_initialized = true;
    }

    QString fragment;
    if (source.contains(QStringLiteral("mainImage"))) {
        fragment = QString::fromUtf8(kShadertoyPrelude) + stripVersionDirective(source)
                 + QString::fromUtf8(kShadertoyEpilogue);
    } else {
        fragment = source;
    }

    auto* program = new QOpenGLShaderProgram();
    if (!program->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader)
        || !program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment)
        || !program->link()) {
        if (errorOut) *errorOut = program->log();
        delete program;
        return false;
    }

    delete m_program;
    m_program = program;

    if (!m_vao) {
        m_vao = new QOpenGLVertexArrayObject();
        m_vao->create();
    }

    resetClock();
    m_clock.start();
    return true;
}

void ShaderRenderer::render(int widthPx, int heightPx)
{
    if (!m_program) return;
    if (!m_initialized) {
        initializeOpenGLFunctions();
        m_initialized = true;
    }

    const qint64 nowNs = m_clock.isValid() ? m_clock.nsecsElapsed() : 0;
    const float time = static_cast<float>(nowNs) / 1.0e9f;
    const float delta = static_cast<float>(nowNs - m_lastNs) / 1.0e9f;
    m_lastNs = nowNs;

    const QDateTime now = QDateTime::currentDateTime();
    const QTime timeOfDay = now.time();

    glViewport(0, 0, widthPx, heightPx);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    m_program->bind();
    m_program->setUniformValue("iResolution",
                               QVector3D(static_cast<float>(widthPx), static_cast<float>(heightPx), 1.0f));
    m_program->setUniformValue("iTime", time);
    m_program->setUniformValue("iTimeDelta", delta > 0.0f ? delta : 1.0f / 60.0f);
    m_program->setUniformValue("iFrameRate", delta > 0.0f ? 1.0f / delta : 60.0f);
    m_program->setUniformValue("iFrame", m_frame);
    m_program->setUniformValue("iMouse", QVector4D(0.0f, 0.0f, 0.0f, 0.0f));
    m_program->setUniformValue("iDate",
                               QVector4D(static_cast<float>(now.date().year()),
                                         static_cast<float>(now.date().month() - 1),
                                         static_cast<float>(now.date().day()),
                                         static_cast<float>(timeOfDay.msecsSinceStartOfDay()) / 1000.0f));

    if (m_vao && m_vao->isCreated()) m_vao->bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (m_vao && m_vao->isCreated()) m_vao->release();

    m_program->release();
    ++m_frame;
}

void ShaderRenderer::cleanup()
{
    if (m_vao) {
        m_vao->destroy();
        delete m_vao;
        m_vao = nullptr;
    }
    delete m_program;
    m_program = nullptr;
}
