#pragma once

namespace render
{
    class Renderer
    {
    public:
        void initialize();
        void resize(int width, int height);
        void renderFrame();

    private:
        int m_width = 1;
        int m_height = 1;
    };
}