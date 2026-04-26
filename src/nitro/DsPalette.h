#pragma once
#include <cstdint>

inline void BGR555ToRGBA(uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a)
{
    r = ((c >> 0) & 0x1F) << 3;
    g = ((c >> 5) & 0x1F) << 3;
    b = ((c >> 10) & 0x1F) << 3;

    a = (c == 0) ? 0 : 255; // DS transparency rule
}