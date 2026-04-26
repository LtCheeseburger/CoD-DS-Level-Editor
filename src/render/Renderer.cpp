#include "Renderer.hpp"

#include <QOpenGLContext>
#include <QOpenGLFunctions>

namespace render
{
    void Renderer::initialize()
    {
        auto* gl = QOpenGLContext::currentContext()->functions();

        gl->glEnable(GL_DEPTH_TEST);
        gl->glClearColor(0.05f, 0.06f, 0.07f, 1.0f);
    }

    void Renderer::resize(int width, int height)
    {
        m_width = width;
        m_height = height;

        auto* gl = QOpenGLContext::currentContext()->functions();
        gl->glViewport(0, 0, m_width, m_height);
    }

    void Renderer::renderFrame()
    {
        auto* gl = QOpenGLContext::currentContext()->functions();

        gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
}