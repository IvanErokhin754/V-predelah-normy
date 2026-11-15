#include "renderer.h"

#include <cmath>
#include <cstdio>

namespace gfx {

// ------------------------------------------------------------------ шейдеры

namespace {

const char* const kWorldVert = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
uniform mat4 uViewProj;
uniform mat4 uModel;
out vec3 vPos;
out vec3 vNormal;
out vec2 vUV;
void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vPos = world.xyz;
    vNormal = mat3(uModel) * aNormal;
    vUV = aUV;
    gl_Position = uViewProj * world;
}
)";

const char* const kWorldFrag = R"(#version 330 core
in vec3 vPos;
in vec3 vNormal;
in vec2 vUV;
uniform sampler2D uTex;
uniform vec3 uTint;
uniform float uEmissive;
uniform float uLit;
uniform int uLightCount;
uniform vec3 uLPos[16];
uniform vec3 uLCol[16];
uniform float uLRad[16];
uniform vec3 uLMin[16];
uniform vec3 uLMax[16];
uniform vec3 uCamPos;
uniform vec3 uAmbient;
uniform float uFog;
out vec4 fragColor;

// Упорядоченный дизеринг 4x4: в темноте градиенты распадаются на зерно,
// а не на ровные полосы.
float bayer2(vec2 a) {
    a = floor(a);
    return fract(a.x / 2.0 + a.y * a.y * 0.75);
}
float bayer4(vec2 a) {
    return bayer2(0.5 * a) * 0.25 + bayer2(a);
}

void main() {
    vec4 tex = texture(uTex, vUV);
    if (tex.a < 0.5) discard;
    vec3 n = normalize(vNormal);
    vec3 light = uAmbient;
    for (int i = 0; i < 16; ++i) {
        if (i >= uLightCount) break;
        // Источник светит только внутри своей коробки (комнаты).
        vec3 inside = step(uLMin[i], vPos) * step(vPos, uLMax[i]);
        float mask = inside.x * inside.y * inside.z;
        vec3 d = uLPos[i] - vPos;
        float dist = length(d);
        float att = clamp(1.0 - dist / uLRad[i], 0.0, 1.0);
        att = att * att;
        float ndl = max(dot(n, d / max(dist, 0.001)), 0.0);
        light += uLCol[i] * (att * mask * (0.22 + 0.78 * ndl));
    }
    vec3 color = tex.rgb * uTint * (light * uLit + vec3(uEmissive));
    float fog = exp(-uFog * distance(uCamPos, vPos));
    color *= mix(1.0, fog, clamp(1.0 - uEmissive * 0.5, 0.0, 1.0));
    float levels = 40.0;
    color = floor(color * levels + bayer4(gl_FragCoord.xy)) / levels;
    fragColor = vec4(color, 1.0);
}
)";

const char* const kUiVert = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
uniform mat4 uProj;
out vec2 vUV;
out vec4 vColor;
void main() {
    vUV = aUV;
    vColor = aColor;
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
}
)";

const char* const kUiFrag = R"(#version 330 core
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTex;
out vec4 fragColor;
void main() {
    float a = texture(uTex, vUV).r;
    fragColor = vec4(vColor.rgb, vColor.a * a);
}
)";

const char* const kFxVert = R"(#version 330 core
layout(location = 0) in vec2 aPos;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* const kFxFrag = R"(#version 330 core
uniform int uMode;
uniform float uTime;
uniform vec2 uRes;
uniform float uParam;
out vec4 fragColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 uv = gl_FragCoord.xy / uRes;
    if (uMode == 0) {
        // Фон терминала (#071009) с низкоконтрастной рябью и бегущими строками.
        vec3 base = vec3(0.027, 0.063, 0.035);
        float n = hash(floor(gl_FragCoord.xy / 2.0) + floor(uTime * 20.0) * 17.0);
        float ripple = (n - 0.5) * 0.030;
        float lines = 0.5 + 0.5 * sin((gl_FragCoord.y + uTime * 22.0) * 1.2);
        float band = max(sin(uv.y * 9.0 + uTime * 0.8), 0.0);
        float fx = ripple + 0.010 * lines + 0.016 * band * band;
        vec3 color = base + vec3(0.30, 1.0, 0.45) * fx * uParam;
        fragColor = vec4(color, 1.0);
    } else {
        // Виньетка и зерно поверх кадра.
        vec2 p = (uv - 0.5) * vec2(1.0, 0.92);
        float v = smoothstep(0.38, 0.98, length(p)) * uParam;
        float g = (hash(gl_FragCoord.xy + fract(uTime) * 91.0) - 0.5) * 0.05;
        fragColor = vec4(0.0, 0.0, 0.0, clamp(v + g, 0.0, 1.0));
    }
}
)";

GLuint compile(GLenum type, const char* src, std::string* error) {
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        GLsizei len = 0;
        glGetShaderInfoLog(s, sizeof(log), &len, log);
        if (error) *error = std::string("ошибка компиляции шейдера: ") + log;
        return 0;
    }
    return s;
}

GLuint link(const char* vert, const char* frag, std::string* error) {
    const GLuint vs = compile(GL_VERTEX_SHADER, vert, error);
    if (!vs) return 0;
    const GLuint fs = compile(GL_FRAGMENT_SHADER, frag, error);
    if (!fs) return 0;
    const GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        GLsizei len = 0;
        glGetProgramInfoLog(p, sizeof(log), &len, log);
        if (error) *error = std::string("ошибка линковки шейдера: ") + log;
        return 0;
    }
    return p;
}

} // namespace

// ----------------------------------------------------------------- текстуры

static void uploadPixels(const Texture& tex, const Image& img) {
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.w, img.h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 img.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
}

Texture makeTexture(const Image& img, bool nearest, bool repeat) {
    Texture t;
    t.w = img.w;
    t.h = img.h;
    glGenTextures(1, &t.id);
    uploadPixels(t, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, nearest ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    nearest ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
    const GLint wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    return t;
}

void updateTexture(Texture& tex, const Image& img) {
    tex.w = img.w;
    tex.h = img.h;
    uploadPixels(tex, img);
}

// --------------------------------------------------------------------- меши

void MeshBuilder::quad(const Vec3& c, const Vec3& r, const Vec3& u, const Vec3& n, float u0,
                       float v0, float u1, float v1) {
    const Vec3 tl = c - r + u;
    const Vec3 tr = c + r + u;
    const Vec3 br = c + r - u;
    const Vec3 bl = c - r - u;
    auto push = [&](const Vec3& p, float tu, float tv) {
        verts_.push_back(Vertex{p.x, p.y, p.z, n.x, n.y, n.z, tu, tv});
    };
    push(tl, u0, v0);
    push(bl, u0, v1);
    push(br, u1, v1);
    push(tl, u0, v0);
    push(br, u1, v1);
    push(tr, u1, v0);
}

void MeshBuilder::box(const Vec3& mn, const Vec3& mx, float tile, unsigned faces) {
    const Vec3 c = (mn + mx) * 0.5f;
    const float hx = (mx.x - mn.x) * 0.5f;
    const float hy = (mx.y - mn.y) * 0.5f;
    const float hz = (mx.z - mn.z) * 0.5f;
    auto face = [&](const Vec3& center, const Vec3& right, const Vec3& up, const Vec3& n) {
        float u1 = 1, v1 = 1;
        if (tile > 0) {
            u1 = 2 * length(right) * tile;
            v1 = 2 * length(up) * tile;
        }
        quad(center, right, up, n, 0, 0, u1, v1);
    };
    // «Вправо» для каждой грани — направление, которое видит зритель снаружи,
    // поэтому надписи на текстурах не получаются зеркальными.
    if (faces & PosX) face({mx.x, c.y, c.z}, {0, 0, -hz}, {0, hy, 0}, {1, 0, 0});
    if (faces & NegX) face({mn.x, c.y, c.z}, {0, 0, hz}, {0, hy, 0}, {-1, 0, 0});
    if (faces & PosY) face({c.x, mx.y, c.z}, {hx, 0, 0}, {0, 0, -hz}, {0, 1, 0});
    if (faces & NegY) face({c.x, mn.y, c.z}, {hx, 0, 0}, {0, 0, hz}, {0, -1, 0});
    if (faces & PosZ) face({c.x, c.y, mx.z}, {hx, 0, 0}, {0, hy, 0}, {0, 0, 1});
    if (faces & NegZ) face({c.x, c.y, mn.z}, {-hx, 0, 0}, {0, hy, 0}, {0, 0, -1});
}

Mesh uploadMesh(const MeshBuilder& b) {
    Mesh m;
    m.count = static_cast<int>(b.vertices().size());
    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);
    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(b.vertices().size() * sizeof(Vertex)),
                 b.vertices().data(), GL_STATIC_DRAW);
    const GLsizei stride = sizeof(Vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(6 * sizeof(float)));
    glBindVertexArray(0);
    return m;
}

// ------------------------------------------------------------------- рендер

bool Renderer::init(std::string* error) {
    worldProg_ = link(kWorldVert, kWorldFrag, error);
    if (!worldProg_) return false;
    uViewProj_ = glGetUniformLocation(worldProg_, "uViewProj");
    uModel_ = glGetUniformLocation(worldProg_, "uModel");
    uTex_ = glGetUniformLocation(worldProg_, "uTex");
    uTint_ = glGetUniformLocation(worldProg_, "uTint");
    uEmissive_ = glGetUniformLocation(worldProg_, "uEmissive");
    uLit_ = glGetUniformLocation(worldProg_, "uLit");
    uLightCount_ = glGetUniformLocation(worldProg_, "uLightCount");
    uLPos_ = glGetUniformLocation(worldProg_, "uLPos[0]");
    uLCol_ = glGetUniformLocation(worldProg_, "uLCol[0]");
    uLRad_ = glGetUniformLocation(worldProg_, "uLRad[0]");
    uLMin_ = glGetUniformLocation(worldProg_, "uLMin[0]");
    uLMax_ = glGetUniformLocation(worldProg_, "uLMax[0]");
    uCamPos_ = glGetUniformLocation(worldProg_, "uCamPos");
    uAmbient_ = glGetUniformLocation(worldProg_, "uAmbient");
    uFog_ = glGetUniformLocation(worldProg_, "uFog");

    uiProg_ = link(kUiVert, kUiFrag, error);
    if (!uiProg_) return false;
    uUiProj_ = glGetUniformLocation(uiProg_, "uProj");
    uUiTex_ = glGetUniformLocation(uiProg_, "uTex");

    fxProg_ = link(kFxVert, kFxFrag, error);
    if (!fxProg_) return false;
    uFxMode_ = glGetUniformLocation(fxProg_, "uMode");
    uFxTime_ = glGetUniformLocation(fxProg_, "uTime");
    uFxRes_ = glGetUniformLocation(fxProg_, "uRes");
    uFxParam_ = glGetUniformLocation(fxProg_, "uParam");

    // Атлас шрифта: все глифы в сетке по 16 в ряд.
    const int count = font::glyphCount();
    atlasRows_ = (count + atlasCols_ - 1) / atlasCols_;
    Image atlas(atlasCols_ * font::kGlyphW, atlasRows_ * font::kGlyphH, Color{0, 0, 0, 255});
    for (int i = 0; i < count; ++i) {
        const uint8_t* rows = font::glyphRows(i);
        const int ox = (i % atlasCols_) * font::kGlyphW;
        const int oy = (i / atlasCols_) * font::kGlyphH;
        for (int y = 0; y < font::kGlyphH; ++y)
            for (int x = 0; x < font::kGlyphW; ++x)
                if (rows[y] & (0x80 >> x)) atlas.set(ox + x, oy + y, Color{255, 255, 255, 255});
    }
    atlas_ = makeTexture(atlas, true, false);
    glBindTexture(GL_TEXTURE_2D, atlas_.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    solidGlyph_ = font::glyphIndex(0x2588); // сплошной блок — для заливок

    glGenVertexArrays(1, &uiVao_);
    glBindVertexArray(uiVao_);
    glGenBuffers(1, &uiVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    const GLsizei stride = 8 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(4 * sizeof(float)));

    glGenVertexArrays(1, &fxVao_);
    glBindVertexArray(fxVao_);
    glGenBuffers(1, &fxVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, fxVbo_);
    const float quadVerts[12] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindVertexArray(0);
    return true;
}

void Renderer::bindScreen(int width, int height) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    targetW_ = width;
    targetH_ = height;
    targetFlip_ = false;
}

void Renderer::bindTarget(const RenderTarget& target) {
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
    glViewport(0, 0, target.color.w, target.color.h);
    targetW_ = target.color.w;
    targetH_ = target.color.h;
    targetFlip_ = true;
}

RenderTarget Renderer::makeTarget(int width, int height) {
    RenderTarget t;
    t.color.w = width;
    t.color.h = height;
    glGenTextures(1, &t.color.id);
    glBindTexture(GL_TEXTURE_2D, t.color.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &t.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.color.id, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::fprintf(stderr, "предупреждение: буфер кадра монитора не собран\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return t;
}

void Renderer::clear(float r, float g, float b) {
    glDepthMask(GL_TRUE);
    glClearColor(r, g, b, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::beginWorld(const Mat4& viewProj, const Vec3& camPos,
                          const std::vector<Light>& lights, const Vec3& ambient,
                          float fogDensity) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glUseProgram(worldProg_);
    glUniformMatrix4fv(uViewProj_, 1, GL_FALSE, viewProj.m);
    glUniform3f(uCamPos_, camPos.x, camPos.y, camPos.z);
    glUniform3f(uAmbient_, ambient.x, ambient.y, ambient.z);
    glUniform1f(uFog_, fogDensity);
    glUniform1i(uTex_, 0);
    glActiveTexture(GL_TEXTURE0);

    float pos[kMaxLights * 3] = {0};
    float col[kMaxLights * 3] = {0};
    float rad[kMaxLights];
    float mn[kMaxLights * 3] = {0};
    float mx[kMaxLights * 3] = {0};
    for (int i = 0; i < kMaxLights; ++i) rad[i] = 1;
    int n = static_cast<int>(lights.size());
    if (n > kMaxLights) n = kMaxLights;
    for (int i = 0; i < n; ++i) {
        const Light& l = lights[i];
        pos[i * 3 + 0] = l.pos.x;
        pos[i * 3 + 1] = l.pos.y;
        pos[i * 3 + 2] = l.pos.z;
        col[i * 3 + 0] = l.color.x;
        col[i * 3 + 1] = l.color.y;
        col[i * 3 + 2] = l.color.z;
        rad[i] = l.radius;
        mn[i * 3 + 0] = l.boxMin.x;
        mn[i * 3 + 1] = l.boxMin.y;
        mn[i * 3 + 2] = l.boxMin.z;
        mx[i * 3 + 0] = l.boxMax.x;
        mx[i * 3 + 1] = l.boxMax.y;
        mx[i * 3 + 2] = l.boxMax.z;
    }
    glUniform1i(uLightCount_, n);
    glUniform3fv(uLPos_, kMaxLights, pos);
    glUniform3fv(uLCol_, kMaxLights, col);
    glUniform1fv(uLRad_, kMaxLights, rad);
    glUniform3fv(uLMin_, kMaxLights, mn);
    glUniform3fv(uLMax_, kMaxLights, mx);
}

void Renderer::drawMesh(const Mesh& mesh, const Texture& tex, const Mat4& model, const Vec3& tint,
                        float emissive, float lit) {
    if (mesh.count == 0) return;
    glUniformMatrix4fv(uModel_, 1, GL_FALSE, model.m);
    glUniform3f(uTint_, tint.x, tint.y, tint.z);
    glUniform1f(uEmissive_, emissive);
    glUniform1f(uLit_, lit);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glBindVertexArray(mesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh.count);
}

// ----------------------------------------------------------------------- 2D

void Renderer::begin2D(int width, int height) {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(uiProg_);
    // В текстуре первая строка — нижняя, поэтому для неё ось Y не переворачиваем:
    // тогда верх нарисованной картинки попадает в начало текстуры (v = 0).
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const Mat4 proj = targetFlip_ ? Mat4::ortho(0, w, 0, h) : Mat4::ortho(0, w, h, 0);
    glUniformMatrix4fv(uUiProj_, 1, GL_FALSE, proj.m);
    glUniform1i(uUiTex_, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_.id);
    uiVerts_.clear();
}

void Renderer::glyphQuad(float x, float y, float w, float h, int glyph, const Rgba& c) {
    const float cw = 1.0f / atlasCols_;
    const float ch = 1.0f / atlasRows_;
    float u0 = (glyph % atlasCols_) * cw;
    float v0 = (glyph / atlasCols_) * ch;
    float u1 = u0 + cw;
    float v1 = v0 + ch;
    if (glyph == solidGlyph_) {
        // Для заливок берём середину сплошного блока, чтобы не зацепить соседей.
        u0 += cw * 0.4f;
        u1 -= cw * 0.4f;
        v0 += ch * 0.4f;
        v1 -= ch * 0.4f;
    }
    const float v[6][4] = {{x, y, u0, v0},         {x, y + h, u0, v1}, {x + w, y + h, u1, v1},
                           {x, y, u0, v0},         {x + w, y + h, u1, v1}, {x + w, y, u1, v0}};
    for (const auto& p : v) {
        uiVerts_.insert(uiVerts_.end(), {p[0], p[1], p[2], p[3], c.r, c.g, c.b, c.a});
    }
}

void Renderer::rect(float x, float y, float w, float h, const Rgba& c) {
    glyphQuad(x, y, w, h, solidGlyph_, c);
}

void Renderer::frame(float x, float y, float w, float h, float t, const Rgba& c) {
    rect(x, y, w, t, c);
    rect(x, y + h - t, w, t, c);
    rect(x, y + t, t, h - 2 * t, c);
    rect(x + w - t, y + t, t, h - 2 * t, c);
}

void Renderer::line(float x0, float y0, float x1, float y1, float thickness, const Rgba& c) {
    const float dx = x1 - x0, dy = y1 - y0;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-4f) return;
    const float nx = -dy / len * thickness * 0.5f;
    const float ny = dx / len * thickness * 0.5f;
    const float cw = 1.0f / atlasCols_;
    const float ch = 1.0f / atlasRows_;
    const float u = (solidGlyph_ % atlasCols_ + 0.5f) * cw;
    const float v = (solidGlyph_ / atlasCols_ + 0.5f) * ch;
    const float p[6][2] = {{x0 + nx, y0 + ny}, {x0 - nx, y0 - ny}, {x1 - nx, y1 - ny},
                           {x0 + nx, y0 + ny}, {x1 - nx, y1 - ny}, {x1 + nx, y1 + ny}};
    for (const auto& q : p) uiVerts_.insert(uiVerts_.end(), {q[0], q[1], u, v, c.r, c.g, c.b, c.a});
}

void Renderer::text(float x, float y, float scale, const Rgba& c, const std::string& s) {
    float pen = x;
    const float gw = font::kGlyphW * scale;
    const float gh = font::kGlyphH * scale;
    for (uint32_t code : font::decodeUtf8(s)) {
        if (code != ' ') glyphQuad(pen, y, gw, gh, font::glyphIndex(code), c);
        pen += gw;
    }
}

void Renderer::textCentered(float cx, float y, float scale, const Rgba& c, const std::string& s) {
    text(std::floor(cx - font::length(s) * font::kGlyphW * scale * 0.5f), y, scale, c, s);
}

void Renderer::end2D() {
    if (uiVerts_.empty()) return;
    glBindVertexArray(uiVao_);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(uiVerts_.size() * sizeof(float)),
                 uiVerts_.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(uiVerts_.size() / 8));
    uiVerts_.clear();
}

// ------------------------------------------------------------------ эффекты

void Renderer::crtBackground(float time, bool ripple) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUseProgram(fxProg_);
    glUniform1i(uFxMode_, 0);
    glUniform1f(uFxTime_, time);
    glUniform2f(uFxRes_, static_cast<float>(targetW_), static_cast<float>(targetH_));
    glUniform1f(uFxParam_, ripple ? 1.0f : 0.0f);
    glBindVertexArray(fxVao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::vignette(float time, float strength) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(fxProg_);
    glUniform1i(uFxMode_, 1);
    glUniform1f(uFxTime_, time);
    glUniform2f(uFxRes_, static_cast<float>(targetW_), static_cast<float>(targetH_));
    glUniform1f(uFxParam_, strength);
    glBindVertexArray(fxVao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

bool Renderer::saveScreenshot(const std::string& path, int width, int height) {
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            const unsigned char* p = &pixels[(static_cast<size_t>(y) * width + x) * 4];
            std::fwrite(p, 1, 3, f);
        }
    }
    std::fclose(f);
    return true;
}

} // namespace gfx
