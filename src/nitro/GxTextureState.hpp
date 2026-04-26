#pragma once

#include <cstdint>

struct GxTextureState
{
    uint32_t currentTexAddr = 0;

    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t format = 0;
};