#pragma once

#include <cstdint>

namespace tama {

// 6x8 bitmap font, ASCII 0x20..0x7E. Each glyph is 6 columns of 8 bits.
// font_6x8_data[ch - 0x20][col] gives a uint8_t where bit N (0=top) is
// the pixel at row N. Column 5 is left blank to act as inter-character space.
extern const uint8_t kFont6x8[96][6];

constexpr int kFontWidth  = 6;
constexpr int kFontHeight = 8;

// Convenience: pixel width for a string at the given integer scale.
int text_width(const char* s, int scale = 1);

}  // namespace tama
