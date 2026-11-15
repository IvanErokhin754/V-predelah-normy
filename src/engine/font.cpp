#include "font.h"

#include <algorithm>

#include "font_data.inc"

Image::Image(int w_, int h_, Color fill) : w(w_), h(h_), rgba(static_cast<size_t>(w_) * h_ * 4) {
    for (int i = 0; i < w * h; ++i) {
        rgba[i * 4 + 0] = fill.r;
        rgba[i * 4 + 1] = fill.g;
        rgba[i * 4 + 2] = fill.b;
        rgba[i * 4 + 3] = fill.a;
    }
}

void Image::set(int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    uint8_t* p = &rgba[(static_cast<size_t>(y) * w + x) * 4];
    p[0] = c.r;
    p[1] = c.g;
    p[2] = c.b;
    p[3] = c.a;
}

Color Image::get(int x, int y) const {
    x = std::clamp(x, 0, w - 1);
    y = std::clamp(y, 0, h - 1);
    const uint8_t* p = &rgba[(static_cast<size_t>(y) * w + x) * 4];
    return Color{p[0], p[1], p[2], p[3]};
}

void Image::fillRect(int x, int y, int rw, int rh, Color c) {
    for (int yy = y; yy < y + rh; ++yy)
        for (int xx = x; xx < x + rw; ++xx) set(xx, yy, c);
}

void Image::frameRect(int x, int y, int rw, int rh, int t, Color c) {
    fillRect(x, y, rw, t, c);
    fillRect(x, y + rh - t, rw, t, c);
    fillRect(x, y, t, rh, c);
    fillRect(x + rw - t, y, t, rh, c);
}

namespace font {

std::vector<uint32_t> decodeUtf8(const std::string& s) {
    std::vector<uint32_t> out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        uint32_t code = '?';
        int extra = 0;
        if (c < 0x80) {
            code = c;
        } else if ((c & 0xE0) == 0xC0) {
            code = c & 0x1F;
            extra = 1;
        } else if ((c & 0xF0) == 0xE0) {
            code = c & 0x0F;
            extra = 2;
        } else if ((c & 0xF8) == 0xF0) {
            code = c & 0x07;
            extra = 3;
        }
        ++i;
        bool valid = true;
        for (int k = 0; k < extra; ++k) {
            if (i >= s.size() || (static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            code = (code << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
            ++i;
        }
        out.push_back(valid ? code : static_cast<uint32_t>('?'));
    }
    return out;
}

int length(const std::string& s) {
    int n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) ++n; // считаем только первые байты символов
    return n;
}

int glyphCount() { return kFontGlyphCount; }

int glyphIndex(uint32_t code) {
    const uint32_t* begin = kFontCodes;
    const uint32_t* end = kFontCodes + kFontGlyphCount;
    const uint32_t* it = std::lower_bound(begin, end, code);
    if (it != end && *it == code) return static_cast<int>(it - begin);
    it = std::lower_bound(begin, end, static_cast<uint32_t>('?'));
    return static_cast<int>(it - begin);
}

const uint8_t* glyphRows(int index) { return kFontRows[index]; }

void drawText(Image& img, int x, int y, int scale, Color c, const std::string& text) {
    int penX = x;
    for (uint32_t code : decodeUtf8(text)) {
        const uint8_t* rows = glyphRows(glyphIndex(code));
        for (int gy = 0; gy < kGlyphH; ++gy)
            for (int gx = 0; gx < kGlyphW; ++gx)
                if (rows[gy] & (0x80 >> gx)) img.fillRect(penX + gx * scale, y + gy * scale, scale, scale, c);
        penX += kGlyphW * scale;
    }
}

void drawTextCentered(Image& img, int cx, int y, int scale, Color c, const std::string& text) {
    drawText(img, cx - length(text) * kGlyphW * scale / 2, y, scale, c, text);
}

std::vector<std::string> wrap(const std::string& text, int maxChars) {
    std::vector<std::string> lines;
    std::string line;
    int lineLen = 0;
    size_t i = 0;
    while (i <= text.size()) {
        // Вырезаем очередное слово (до пробела или перевода строки).
        size_t j = i;
        while (j < text.size() && text[j] != ' ' && text[j] != '\n') ++j;
        const std::string word = text.substr(i, j - i);
        const int wordLen = length(word);
        if (lineLen > 0 && lineLen + 1 + wordLen > maxChars) {
            lines.push_back(line);
            line.clear();
            lineLen = 0;
        }
        if (wordLen > 0) {
            if (lineLen > 0) {
                line += ' ';
                ++lineLen;
            }
            line += word;
            lineLen += wordLen;
        }
        if (j < text.size() && text[j] == '\n') {
            lines.push_back(line);
            line.clear();
            lineLen = 0;
        }
        if (j >= text.size()) break;
        i = j + 1;
    }
    if (!line.empty() || lines.empty()) lines.push_back(line);
    return lines;
}

} // namespace font
