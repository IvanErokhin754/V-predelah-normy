// Рендер на OpenGL 3.3 core: меши с точечными источниками света,
// 2D-слой (текст, прямоугольники, линии) и рендер в текстуру для монитора.
#pragma once

#include <string>
#include <vector>

#include "font.h"
#include "gl_min.h"
#include "math3d.h"

namespace gfx {

struct Texture {
    GLuint id = 0;
    int w = 0, h = 0;
};

// nearest — видимые квадратные пиксели (грубые текстуры); repeat — тайлинг.
Texture makeTexture(const Image& img, bool nearest, bool repeat);
void updateTexture(Texture& tex, const Image& img);

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

// Собирает геометрию из прямоугольников и коробок.
class MeshBuilder {
public:
    // Прямоугольник: центр, половина ширины (вектор вправо для зрителя),
    // половина высоты (вектор вверх), нормаль и область текстуры.
    void quad(const Vec3& center, const Vec3& halfRight, const Vec3& halfUp, const Vec3& normal,
              float u0, float v0, float u1, float v1);

    enum Face { PosX = 1, NegX = 2, PosY = 4, NegY = 8, PosZ = 16, NegZ = 32, AllFaces = 63 };

    // Коробка. tile == 0: каждая грань показывает текстуру целиком;
    // tile > 0: текстура повторяется, tile — число повторов на метр.
    void box(const Vec3& mn, const Vec3& mx, float tile = 0, unsigned faces = AllFaces);

    const std::vector<Vertex>& vertices() const { return verts_; }
    bool empty() const { return verts_.empty(); }
    void clear() { verts_.clear(); }

private:
    std::vector<Vertex> verts_;
};

struct Mesh {
    GLuint vao = 0, vbo = 0;
    int count = 0;
};

Mesh uploadMesh(const MeshBuilder& b);

// Точечный источник. Светит только внутри своей коробки: так свет не
// «протекает» сквозь стены без карт теней, а темнота остаётся глубокой.
struct Light {
    Vec3 pos;
    Vec3 color;
    float radius = 5;
    Vec3 boxMin{-1000, -1000, -1000};
    Vec3 boxMax{1000, 1000, 1000};
};

const int kMaxLights = 16;

struct RenderTarget {
    GLuint fbo = 0;
    Texture color;
};

struct Rgba {
    float r = 1, g = 1, b = 1, a = 1;
};

class Renderer {
public:
    bool init(std::string* error);

    // --- куда рисуем ---
    void bindScreen(int width, int height);
    void bindTarget(const RenderTarget& target);
    RenderTarget makeTarget(int width, int height);
    void clear(float r, float g, float b);

    // --- 3D ---
    void beginWorld(const Mat4& viewProj, const Vec3& camPos, const std::vector<Light>& lights,
                    const Vec3& ambient, float fogDensity);
    // emissive — собственное свечение (лампы, экран); lit — доля внешнего света.
    void drawMesh(const Mesh& mesh, const Texture& tex, const Mat4& model,
                  const Vec3& tint = Vec3{1, 1, 1}, float emissive = 0, float lit = 1);

    // --- 2D: координаты в пикселях, начало в левом верхнем углу ---
    void begin2D(int width, int height);
    void rect(float x, float y, float w, float h, const Rgba& c);
    void frame(float x, float y, float w, float h, float thickness, const Rgba& c);
    void line(float x0, float y0, float x1, float y1, float thickness, const Rgba& c);
    void text(float x, float y, float scale, const Rgba& c, const std::string& s);
    void textCentered(float cx, float y, float scale, const Rgba& c, const std::string& s);
    void end2D();

    // --- полноэкранные эффекты ---
    // Фон терминала: почти чёрный зелёный, рябь и бегущие строки под текстом.
    void crtBackground(float time, bool ripple);
    // Виньетка и зерно поверх готового кадра.
    void vignette(float time, float strength);

    bool saveScreenshot(const std::string& path, int width, int height);

private:
    void glyphQuad(float x, float y, float w, float h, int glyph, const Rgba& c);

    GLuint worldProg_ = 0;
    GLint uViewProj_ = -1, uModel_ = -1, uTex_ = -1, uTint_ = -1, uEmissive_ = -1, uLit_ = -1;
    GLint uLightCount_ = -1, uLPos_ = -1, uLCol_ = -1, uLRad_ = -1, uLMin_ = -1, uLMax_ = -1;
    GLint uCamPos_ = -1, uAmbient_ = -1, uFog_ = -1;

    GLuint uiProg_ = 0;
    GLint uUiProj_ = -1, uUiTex_ = -1;
    GLuint uiVao_ = 0, uiVbo_ = 0;
    std::vector<float> uiVerts_;
    Texture atlas_;
    int atlasCols_ = 16, atlasRows_ = 1;
    int solidGlyph_ = 0;

    GLuint fxProg_ = 0;
    GLint uFxMode_ = -1, uFxTime_ = -1, uFxRes_ = -1, uFxParam_ = -1;
    GLuint fxVao_ = 0, fxVbo_ = 0;

    int targetW_ = 1, targetH_ = 1;
    bool targetFlip_ = false; // рисуем в текстуру: ось Y перевёрнута
};

} // namespace gfx
