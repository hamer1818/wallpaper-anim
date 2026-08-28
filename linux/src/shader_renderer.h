#pragma once

#include <QElapsedTimer>
#include <QOpenGLFunctions>
#include <QString>

class QOpenGLShaderProgram;
class QOpenGLVertexArrayObject;

// Draws a Shadertoy-style fragment shader over a fullscreen triangle.
//
// The Windows build renders .hlsl files through D3D11; there is no HLSL compiler on
// Linux, so the Linux port speaks GLSL instead (.glsl / .frag / .fs). Sources may be
// either a complete fragment shader or a Shadertoy "mainImage" body, which gets
// wrapped with the usual iResolution/iTime uniforms.
class ShaderRenderer : protected QOpenGLFunctions {
public:
    ShaderRenderer();
    ~ShaderRenderer();

    // Compiles the shader at filePath. Requires a current GL context.
    bool load(const QString& filePath, QString* errorOut = nullptr);
    void render(int widthPx, int heightPx);
    // Frees GL objects. Requires a current GL context.
    void cleanup();

    bool isValid() const { return m_program != nullptr; }
    void resetClock();

private:
    QOpenGLShaderProgram* m_program = nullptr;
    QOpenGLVertexArrayObject* m_vao = nullptr;
    QElapsedTimer m_clock;
    int m_frame = 0;
    qint64 m_lastNs = 0;
    bool m_initialized = false;
};
