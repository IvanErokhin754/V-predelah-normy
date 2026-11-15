#include "texgen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace texgen {

namespace {

uint32_t hashU(int x, int y, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u +
                 seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float rnd(int x, int y, uint32_t seed) { return (hashU(x, y, seed) & 0xFFFFFF) / 16777216.0f; }

// Плавный шум, который бесшовно повторяется с периодом period (в узлах решётки).
float vnoise(float x, float y, uint32_t seed, int period) {
    const int xi = static_cast<int>(std::floor(x));
    const int yi = static_cast<int>(std::floor(y));
    const float fx = x - xi, fy = y - yi;
    auto at = [&](int ix, int iy) {
        ix = ((ix % period) + period) % period;
        iy = ((iy % period) + period) % period;
        return rnd(ix, iy, seed);
    };
    const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    const float a = at(xi, yi) + (at(xi + 1, yi) - at(xi, yi)) * sx;
    const float b = at(xi, yi + 1) + (at(xi + 1, yi + 1) - at(xi, yi + 1)) * sx;
    return a + (b - a) * sy;
}

// Ступенчатая яркость — так получаются «пиксельные» пятна вместо плавных переходов.
float quant(float v, int levels) { return std::floor(v * levels) / (levels - 1); }

uint8_t toByte(float v) { return static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f)); }

Color mul(Color c, float f) { return Color{toByte(c.r * f), toByte(c.g * f), toByte(c.b * f), c.a}; }

Color mix(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Color{toByte(a.r + (b.r - a.r) * t), toByte(a.g + (b.g - a.g) * t),
                 toByte(a.b + (b.b - a.b) * t), 255};
}

// Шумная металлическая или пластиковая поверхность.
void fillNoisy(Image& img, Color base, uint32_t seed, float amount, int levels = 5) {
    for (int y = 0; y < img.h; ++y)
        for (int x = 0; x < img.w; ++x) {
            const float n = vnoise(x / 8.0f, y / 8.0f, seed, std::max(1, img.w / 8));
            const float s = rnd(x, y, seed + 9);
            img.set(x, y, mul(base, 1.0f - amount + amount * 1.6f * quant(n, levels) + (s - 0.5f) * amount * 0.5f));
        }
}

void rivet(Image& img, int x, int y, Color c) {
    img.fillRect(x, y, 2, 2, mul(c, 1.35f));
    img.set(x + 1, y + 1, mul(c, 0.5f));
}

// Потёки ржавчины сверху вниз.
void rustStreaks(Image& img, uint32_t seed, int count, Color rust) {
    for (int i = 0; i < count; ++i) {
        const int x = static_cast<int>(rnd(i, 1, seed) * img.w);
        const int y0 = static_cast<int>(rnd(i, 2, seed) * img.h * 0.7f);
        const int len = 6 + static_cast<int>(rnd(i, 3, seed) * img.h * 0.3f);
        for (int k = 0; k < len; ++k) {
            const float t = 1.0f - static_cast<float>(k) / len;
            img.set(x, y0 + k, mix(img.get(x, y0 + k), rust, 0.55f * t));
        }
    }
}

std::string num(int v) { return std::to_string(v); }

const Color kInk{24, 26, 24, 255};

} // namespace

// ------------------------------------------------------------------- грубые

Image concrete() {
    Image img(64, 64);
    const Color base{120, 122, 116, 255};
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const float big = vnoise(x / 16.0f, y / 16.0f, 11, 4);
            const float fine = vnoise(x / 4.0f, y / 4.0f, 12, 16);
            float shade = 0.72f + 0.42f * quant(0.6f * big + 0.4f * fine, 6);
            shade += (rnd(x, y, 13) - 0.5f) * 0.07f;
            if (vnoise(x / 20.0f + 3, y / 12.0f, 14, 3) > 0.68f) shade *= 0.78f; // подтёки
            img.set(x, y, mul(base, shade));
        }
    // Сколы и раковины: тёмные квадратные пиксели.
    for (int i = 0; i < 26; ++i) {
        const int x = static_cast<int>(rnd(i, 0, 15) * 62);
        const int y = static_cast<int>(rnd(i, 1, 15) * 62);
        const int s = 1 + static_cast<int>(rnd(i, 2, 15) * 2.2f);
        img.fillRect(x, y, s, s, mul(base, 0.42f));
        img.set(x + s, y + s, mul(base, 1.12f));
    }
    // Шов опалубки.
    for (int x = 0; x < 64; ++x) img.set(x, 31, mul(img.get(x, 31), 0.7f));
    return img;
}

Image paint() {
    Image img(64, 64);
    const Color green{74, 96, 82, 255};
    const Color bare{116, 118, 112, 255};
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const float n = vnoise(x / 8.0f, y / 8.0f, 21, 8);
            float shade = 0.8f + 0.3f * quant(n, 4) + (rnd(x, y, 22) - 0.5f) * 0.05f;
            shade *= 1.0f - 0.25f * (y / 63.0f); // к полу грязнее
            Color c = mul(green, shade);
            // Облупившаяся краска: чаще у пола.
            const float chip = vnoise(x / 5.0f, y / 5.0f, 23, 12) + (y / 63.0f) * 0.25f;
            if (chip > 0.86f) c = mul(bare, 0.75f + 0.2f * rnd(x, y, 24));
            img.set(x, y, c);
        }
    for (int x = 0; x < 64; ++x) {
        img.set(x, 0, Color{30, 38, 33, 255}); // кромка окраски
        img.set(x, 1, mul(green, 1.25f));
        img.set(x, 62, mul(img.get(x, 62), 0.6f));
        img.set(x, 63, Color{22, 24, 22, 255}); // плинтус
    }
    return img;
}

Image floorTiles() {
    Image img(64, 64);
    const Color base{92, 88, 80, 255};
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const int tx = x / 32, ty = y / 32;
            float shade = 0.82f + 0.16f * rnd(tx, ty, 31); // плитки чуть разного тона
            shade += 0.22f * quant(vnoise(x / 8.0f, y / 8.0f, 32, 8), 5) - 0.1f;
            shade += (rnd(x, y, 33) - 0.5f) * 0.06f;
            if (x % 32 == 0 || y % 32 == 0) shade *= 0.5f; // швы
            img.set(x, y, mul(base, shade));
        }
    // Потёртости.
    for (int i = 0; i < 14; ++i) {
        const int x = static_cast<int>(rnd(i, 0, 34) * 56);
        const int y = static_cast<int>(rnd(i, 1, 34) * 60);
        const int len = 3 + static_cast<int>(rnd(i, 2, 34) * 6);
        for (int k = 0; k < len; ++k) img.set(x + k, y + k / 3, mul(img.get(x + k, y + k / 3), 0.72f));
    }
    return img;
}

Image ceiling() {
    Image img(64, 64);
    const Color base{84, 86, 84, 255};
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            float shade = 0.75f + 0.3f * quant(vnoise(x / 16.0f, y / 16.0f, 41, 4), 4);
            shade += (rnd(x, y, 42) - 0.5f) * 0.06f;
            if (x % 32 < 1 || y % 32 < 1) shade *= 0.6f;
            if (vnoise(x / 10.0f, y / 10.0f, 43, 6) > 0.75f) shade *= 0.7f; // пятна сырости
            img.set(x, y, mul(base, shade));
        }
    return img;
}

// ------------------------------------------------------------------ предметы

static Image doorBase(Color base, uint32_t seed) {
    Image img(64, 128);
    fillNoisy(img, base, seed, 0.22f);
    img.frameRect(0, 0, 64, 128, 2, mul(base, 0.5f));
    img.frameRect(4, 4, 56, 120, 1, mul(base, 0.7f));
    for (int y = 8; y < 124; y += 14) {
        rivet(img, 6, y, base);
        rivet(img, 56, y, base);
    }
    rustStreaks(img, seed + 1, 12, Color{110, 62, 36, 255});
    return img;
}

Image cellDoor() {
    Image img = doorBase(Color{104, 112, 108, 255}, 51);
    // Смотровая щель с прутьями.
    img.fillRect(18, 22, 28, 12, Color{10, 12, 12, 255});
    img.frameRect(17, 21, 30, 14, 1, Color{48, 52, 50, 255});
    for (int x = 22; x < 46; x += 5) img.fillRect(x, 22, 1, 12, Color{70, 74, 72, 255});
    // Лючок для еды.
    img.frameRect(16, 78, 32, 10, 1, Color{44, 48, 46, 255});
    img.fillRect(30, 82, 4, 2, Color{150, 150, 140, 255});
    // Предупреждающая полоса внизу.
    for (int y = 112; y < 122; ++y)
        for (int x = 5; x < 59; ++x)
            img.set(x, y, ((x + y) / 5) % 2 ? Color{168, 138, 40, 255} : Color{30, 30, 28, 255});
    return img;
}

Image heavyDoor() {
    Image img = doorBase(Color{84, 70, 66, 255}, 61);
    rustStreaks(img, 62, 26, Color{120, 52, 30, 255});
    img.fillRect(5, 58, 54, 8, Color{120, 28, 24, 255}); // красная полоса
    for (int x = 8; x < 58; x += 12) rivet(img, x, 61, Color{120, 28, 24, 255});
    img.frameRect(22, 14, 20, 20, 2, Color{40, 34, 32, 255});
    img.fillRect(24, 16, 16, 16, Color{16, 14, 14, 255});
    return img;
}

Image wingDoor() {
    Image img = doorBase(Color{96, 100, 104, 255}, 71);
    img.fillRect(6, 30, 52, 20, Color{176, 172, 150, 255});
    font::drawTextCentered(img, 32, 32, 1, kInk, "КРЫЛО Д");
    img.fillRect(14, 58, 36, 18, Color{150, 44, 34, 255});
    font::drawTextCentered(img, 32, 59, 1, Color{230, 220, 200, 255}, "ПУСТО");
    return img;
}

Image steel() {
    Image img(32, 32);
    fillNoisy(img, Color{96, 100, 100, 255}, 81, 0.25f);
    rustStreaks(img, 82, 5, Color{108, 60, 36, 255});
    return img;
}

Image blanket() {
    Image img(64, 64);
    const Color base{84, 92, 110, 255};
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            float shade = 0.85f + 0.2f * quant(vnoise(x / 8.0f, y / 8.0f, 91, 8), 4);
            if ((x + y) % 2 == 0) shade *= 0.94f; // плетение
            Color c = mul(base, shade);
            if (y % 32 > 24 && y % 32 < 29) c = mul(Color{150, 150, 140, 255}, shade); // полоса
            img.set(x, y, c);
        }
    return img;
}

Image pillow() {
    Image img(32, 32);
    fillNoisy(img, Color{150, 146, 132, 255}, 101, 0.15f, 4);
    img.frameRect(0, 0, 32, 32, 1, Color{104, 100, 90, 255});
    return img;
}

Image robotBody() {
    Image img(128, 128);
    const Color base{98, 106, 84, 255};
    fillNoisy(img, base, 111, 0.2f);
    // Листы обшивки и болты.
    for (int y = 0; y < 128; y += 32)
        for (int x = 0; x < 128; x += 32) {
            img.frameRect(x, y, 32, 32, 1, mul(base, 0.55f));
            rivet(img, x + 3, y + 3, base);
            rivet(img, x + 27, y + 3, base);
            rivet(img, x + 3, y + 27, base);
            rivet(img, x + 27, y + 27, base);
        }
    // Предупреждающие шевроны.
    for (int y = 96; y < 108; ++y)
        for (int x = 0; x < 128; ++x)
            img.set(x, y, ((x + y) / 6) % 2 ? Color{176, 144, 40, 255} : Color{28, 28, 26, 255});
    // Трафаретный номер.
    font::drawText(img, 40, 40, 2, Color{214, 210, 190, 255}, "К-07");
    rustStreaks(img, 112, 18, Color{104, 60, 34, 255});
    return img;
}

Image robotDark() {
    Image img(64, 64);
    fillNoisy(img, Color{52, 54, 54, 255}, 121, 0.3f);
    for (int y = 0; y < 64; y += 8) img.fillRect(0, y, 64, 1, Color{24, 24, 24, 255}); // траки
    return img;
}

Image crate() {
    Image img(128, 128);
    const Color wood{128, 104, 72, 255};
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            const int plank = y / 21;
            float shade = 0.8f + 0.2f * rnd(plank, 0, 131);
            shade += 0.18f * quant(vnoise(x / 20.0f, y / 3.0f + plank * 7, 132, 6), 5) - 0.08f;
            if (y % 21 == 0) shade *= 0.45f; // щели между досками
            img.set(x, y, mul(wood, shade));
        }
    // Металлическая окантовка.
    const Color metal{70, 74, 74, 255};
    img.frameRect(0, 0, 128, 128, 6, metal);
    for (int i = 10; i < 128; i += 18) {
        rivet(img, i, 2, metal);
        rivet(img, i, 123, metal);
        rivet(img, 2, i, metal);
        rivet(img, 123, i, metal);
    }
    return img;
}

Image tableTop() {
    Image img(64, 64);
    fillNoisy(img, Color{88, 84, 78, 255}, 141, 0.2f);
    img.frameRect(0, 0, 64, 64, 2, Color{46, 44, 42, 255});
    return img;
}

Image monitorCase() {
    Image img(64, 64);
    fillNoisy(img, Color{168, 160, 136, 255}, 151, 0.12f, 4);
    for (int y = 12; y < 52; y += 4) img.fillRect(10, y, 44, 1, Color{70, 66, 56, 255}); // вентиляция
    return img;
}

Image keyboard() {
    Image img(128, 48, Color{120, 114, 98, 255});
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 14; ++col) {
            const int x = 4 + col * 8 + row * 2, y = 6 + row * 9;
            if (x + 7 > 124) continue;
            img.fillRect(x, y, 7, 8, Color{58, 56, 52, 255});
            img.fillRect(x + 1, y + 1, 5, 5, Color{196, 190, 170, 255});
        }
    img.fillRect(34, 42, 60, 5, Color{196, 190, 170, 255}); // пробел
    return img;
}

Image speaker() {
    Image img(64, 64);
    fillNoisy(img, Color{72, 76, 76, 255}, 161, 0.2f);
    img.frameRect(0, 0, 64, 64, 2, Color{36, 38, 38, 255});
    for (int y = 10; y < 54; y += 4)
        for (int x = 10; x < 54; x += 4) img.fillRect(x, y, 2, 2, Color{14, 14, 14, 255});
    return img;
}

Image cloth(Color base, Color stripe) {
    Image img(32, 32);
    fillNoisy(img, base, 171, 0.18f, 4);
    img.fillRect(0, 20, 32, 3, stripe);
    return img;
}

Image flat(Color c) { return Image(8, 8, c); }

Image robotEye() {
    Image img(64, 16);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 64; ++x) {
            const float dx = (x - 31.5f) / 32.0f, dy = (y - 7.5f) / 8.0f;
            float v = 1.0f - 0.45f * (dx * dx) - 0.3f * (dy * dy); // ярче в центре
            if (y % 3 == 2) v *= 0.45f;                             // строки развёртки
            const float r = std::sqrt((x - 31.5f) * (x - 31.5f) + (y - 7.5f) * (y - 7.5f) * 4.0f);
            if (r < 5.5f) v = 0.12f;                    // тёмный объектив
            if (r < 2.0f) v = 1.0f;                     // и точка в нём
            if (r >= 5.5f && r < 7.0f) v = std::min(1.0f, v * 1.25f);
            v *= 0.9f + 0.2f * rnd(x, y, 251);
            const uint8_t c = static_cast<uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f);
            img.set(x, y, Color{c, c, c, 255});
        }
    return img;
}

Image lightSpill() {
    Image img(32, 16, Color{0, 0, 0, 0});
    static const float bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 32; ++x) {
            const float depth = 1.0f - y / 16.0f;                       // от двери вглубь
            const float edge = std::min(1.0f, std::min(x, 31 - x) / 5.0f); // края щели темнее
            float v = depth * depth * (0.45f + 0.55f * edge);
            v *= 0.85f + 0.3f * (rnd(x, y, 241) - 0.5f);
            const float threshold = (bayer[y % 4][x % 4] + 0.5f) / 16.0f;
            if (v * 1.6f < threshold) continue; // прозрачный пиксель: край рассыпается
            const float level = std::floor(std::min(1.0f, v * 1.3f) * 5.0f) / 5.0f + 0.12f;
            const uint8_t c = static_cast<uint8_t>(std::min(255.0f, level * 255.0f));
            img.set(x, y, Color{c, c, c, 255});
        }
    return img;
}

// -------------------------------------------------------------------- детали

Image monitorBezel() {
    // 512 px на 46 см корпуса: самая плотная текстура в сцене.
    Image img(512, 448);
    const Color plastic{176, 168, 142, 255};
    fillNoisy(img, plastic, 181, 0.08f, 5);
    img.frameRect(0, 0, 512, 448, 6, mul(plastic, 0.6f));
    // Тёмная рамка вокруг кинескопа (сам экран рисуется отдельной гранью).
    img.fillRect(40, 30, 432, 326, Color{26, 26, 24, 255});
    img.frameRect(40, 30, 432, 326, 5, Color{58, 56, 50, 255});
    // Нижняя панель: шильдик, ручки, индикатор.
    img.fillRect(44, 388, 150, 36, Color{40, 40, 38, 255});
    font::drawText(img, 52, 390, 2, Color{208, 200, 170, 255}, "ЭЛТ-14М");
    for (int i = 0; i < 3; ++i) {
        const int x = 300 + i * 46;
        img.fillRect(x, 392, 28, 28, Color{48, 46, 42, 255});
        img.fillRect(x + 4, 396, 20, 20, Color{96, 92, 82, 255});
        img.fillRect(x + 12, 396, 4, 10, Color{210, 204, 180, 255});
    }
    img.fillRect(452, 400, 14, 14, Color{20, 20, 18, 255});
    img.fillRect(455, 403, 8, 8, Color{120, 255, 140, 255}); // индикатор питания
    font::drawText(img, 216, 402, 1, mul(plastic, 0.45f), "СЕТЬ");
    // Вентиляционные прорези сверху.
    for (int x = 120; x < 392; x += 10) img.fillRect(x, 12, 5, 10, mul(plastic, 0.5f));
    // Царапины.
    for (int i = 0; i < 40; ++i) {
        const int x = static_cast<int>(rnd(i, 0, 182) * 500);
        const int y = 376 + static_cast<int>(rnd(i, 1, 182) * 66);
        const int len = 4 + static_cast<int>(rnd(i, 2, 182) * 22);
        for (int k = 0; k < len; ++k) img.set(x + k, y, mul(img.get(x + k, y), 0.82f));
    }
    return img;
}

Image numberPlate(int cell, int prisoner) {
    Image img(256, 128);
    const Color enamel{206, 202, 184, 255};
    fillNoisy(img, enamel, 191, 0.06f, 5);
    img.frameRect(0, 0, 256, 128, 4, Color{40, 42, 40, 255});
    img.frameRect(8, 8, 240, 112, 2, kInk);
    font::drawTextCentered(img, 128, 16, 3, kInk, "КАМЕРА " + num(cell));
    font::drawTextCentered(img, 128, 68, 3, kInk, "№ " + num(prisoner));
    // Винты и сколы эмали по углам.
    const int corners[4][2] = {{12, 12}, {236, 12}, {12, 108}, {236, 108}};
    for (const auto& c : corners) {
        img.fillRect(c[0], c[1], 8, 8, Color{90, 92, 88, 255});
        img.fillRect(c[0] + 2, c[1] + 3, 4, 2, Color{30, 30, 30, 255});
    }
    for (int i = 0; i < 60; ++i) {
        const int x = static_cast<int>(rnd(i, 0, 192) * 250);
        const int y = static_cast<int>(rnd(i, 1, 192) * 122);
        if (x > 30 && x < 226 && y > 14 && y < 112) continue; // сколы только у краёв
        img.fillRect(x, y, 3, 3, Color{96, 60, 40, 255});
    }
    return img;
}

Image buttonLabel(const std::string& text, Color bg) {
    Image img(256, 96);
    fillNoisy(img, bg, 201, 0.1f, 5);
    img.frameRect(0, 0, 256, 96, 5, mul(bg, 0.45f));
    img.frameRect(5, 5, 246, 86, 2, mul(bg, 1.3f));
    font::drawTextCentered(img, 128, 32, 2, Color{236, 232, 214, 255}, text);
    return img;
}

Image controlPanel(const std::string& title) {
    Image img(256, 256);
    const Color metal{92, 98, 98, 255};
    fillNoisy(img, metal, 211, 0.16f, 5);
    img.frameRect(0, 0, 256, 256, 6, mul(metal, 0.5f));
    const int screws[4][2] = {{10, 10}, {238, 10}, {10, 238}, {238, 238}};
    for (const auto& s : screws) {
        img.fillRect(s[0], s[1], 8, 8, mul(metal, 1.4f));
        img.fillRect(s[0] + 1, s[1] + 3, 6, 2, mul(metal, 0.4f));
    }
    img.fillRect(24, 18, 208, 28, Color{28, 30, 30, 255});
    font::drawTextCentered(img, 128, 24, 1, Color{208, 204, 180, 255}, title);
    rustStreaks(img, 212, 14, Color{104, 60, 36, 255});
    return img;
}

Image invoice(int crateNo, int sealNo) {
    Image img(256, 320);
    const Color paper{206, 200, 176, 255};
    fillNoisy(img, paper, 221, 0.06f, 5);
    img.frameRect(0, 0, 256, 320, 2, Color{120, 114, 96, 255});
    font::drawTextCentered(img, 128, 14, 2, kInk, "НАКЛАДНАЯ");
    img.fillRect(16, 52, 224, 2, kInk);
    font::drawText(img, 20, 66, 2, kInk, "ЯЩИК № " + num(crateNo));
    font::drawText(img, 20, 112, 2, kInk, "ПЛОМБА №");
    font::drawTextCentered(img, 128, 150, 4, kInk, num(sealNo));
    img.frameRect(56, 144, 144, 76, 3, kInk);
    font::drawText(img, 20, 236, 1, kInk, "Сверить пломбу с накладной.");
    font::drawText(img, 20, 256, 1, kInk, "Только указанный ящик.");
    // Подпись и печать.
    for (int x = 0; x < 70; ++x)
        img.fillRect(150 + x, 292 + static_cast<int>(6 * std::sin(x * 0.45f)), 2, 2, Color{40, 50, 110, 255});
    img.frameRect(24, 276, 40, 34, 2, Color{130, 40, 40, 255});
    return img;
}

Image powerPanel(int sector, int occupied, bool active) {
    Image img = controlPanel("ПИТАНИЕ · КРЫЛО Д");
    const Color screen{18, 14, 12, 255};
    const Color led = active ? Color{238, 70, 48, 255} : Color{86, 40, 32, 255};
    const Color ledDim = active ? Color{150, 44, 32, 255} : Color{60, 30, 26, 255};
    // Табло сектора.
    img.fillRect(28, 60, 200, 92, screen);
    img.frameRect(28, 60, 200, 92, 3, Color{50, 52, 52, 255});
    font::drawText(img, 40, 68, 2, led, "СЕКТОР Д-" + std::to_string(sector));
    font::drawText(img, 40, 104, 2, occupied > 0 ? led : ledDim, "ЗАНЯТО: " + std::to_string(occupied));
    if (active && occupied > 0) img.fillRect(204, 110, 12, 12, led);
    // Схема линии: шина и отводы.
    const Color wire{40, 44, 44, 255};
    img.fillRect(28, 170, 200, 4, wire);
    for (int i = 0; i < 5; ++i) {
        const int x = 40 + i * 44;
        img.fillRect(x, 170, 4, 22, wire);
        img.fillRect(x - 6, 192, 16, 10, i == 2 ? led : Color{70, 74, 72, 255});
    }
    img.fillRect(24, 214, 208, 24, Color{168, 138, 40, 255});
    font::drawTextCentered(img, 128, 218, 1, kInk, "ПЛАНОВОЕ ОТКЛЮЧЕНИЕ");
    return img;
}

Image sealTag(int sealNo) {
    Image img(128, 64);
    const Color tag{196, 160, 52, 255};
    fillNoisy(img, tag, 231, 0.08f, 5);
    img.frameRect(0, 0, 128, 64, 3, Color{60, 48, 16, 255});
    font::drawText(img, 8, 6, 1, kInk, "ПЛОМБА");
    font::drawTextCentered(img, 76, 12, 3, kInk, num(sealNo));
    img.fillRect(8, 32, 14, 14, Color{150, 36, 30, 255}); // сургуч
    return img;
}

Image crateLabel(int crateNo) {
    Image img(128, 64);
    fillNoisy(img, Color{198, 194, 174, 255}, 241, 0.06f, 5);
    img.frameRect(0, 0, 128, 64, 3, kInk);
    font::drawTextCentered(img, 64, 6, 1, kInk, "ЯЩИК");
    font::drawTextCentered(img, 64, 24, 2, kInk, "№ " + num(crateNo));
    return img;
}

} // namespace texgen
