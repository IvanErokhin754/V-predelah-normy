// Пиксельный моноширинный шрифт 8x16 с кириллицей (глифы GNU Unifont)
// и простое изображение в памяти, на котором можно рисовать текст.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
};

struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;

    Image() = default;
    Image(int w_, int h_, Color fill = Color{0, 0, 0, 255});

    void set(int x, int y, Color c);
    Color get(int x, int y) const;
    void fillRect(int x, int y, int rw, int rh, Color c);
    void frameRect(int x, int y, int rw, int rh, int thickness, Color c);
};

namespace font {

const int kGlyphW = 8;
const int kGlyphH = 16;

// Разбирает UTF-8 в коды символов. Некорректные байты заменяются на '?'.
std::vector<uint32_t> decodeUtf8(const std::string& s);
// Число символов (не байт) в строке UTF-8.
int length(const std::string& s);

int glyphCount();
// Индекс глифа для символа; для неизвестных символов — индекс '?'.
int glyphIndex(uint32_t code);
// 16 байт глифа: байт на строку, старший бит — левый пиксель.
const uint8_t* glyphRows(int index);

// Рисует строку на изображении; scale — целый множитель пикселя.
void drawText(Image& img, int x, int y, int scale, Color c, const std::string& text);
// То же, но по центру относительно cx.
void drawTextCentered(Image& img, int cx, int y, int scale, Color c, const std::string& text);

// Переносит текст по словам, чтобы строки были не длиннее maxChars.
std::vector<std::string> wrap(const std::string& text, int maxChars);

} // namespace font
