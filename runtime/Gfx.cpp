#include "Gfx.h"

#include <glad/gl.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

#include "stb_image_write.h"
#include "stb_truetype.h"

namespace scratch {

static const char* kVertexShader = R"(#version 330
in vec2 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = vertexNormal;
    gl_Position = mvp * vec4(vertexPosition, 0.0, 1.0);
})";

static const char* kDefaultFragment = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
void main() {
    vec4 c = texture(texture0, fragTexCoord) * fragColor;
    finalColor = vec4(c.rgb * c.a, c.a);
})";

struct GfxState
{
    unsigned vao = 0, vbo = 0, white = 0;
    std::vector<gfx::Vertex> verts;
    Shader defaultShader;
    Shader* shader = nullptr;
    unsigned texture = 0;
    RenderTarget* target = nullptr;
    int fbW = 1, fbH = 1;
    int projVersion = 0;
    float mvp[16] = {};
    bool scissor = false;
    Rect scissorRect;

    static void ApplyProjection(Shader& s);
    static void InitShader(Shader& s);
};
static GfxState g;

void GfxState::ApplyProjection(Shader& s)
{
    if (s.projVersion_ == g.projVersion) return;
    glUniformMatrix4fv(s.mvpLoc_, 1, GL_FALSE, g.mvp);
    s.projVersion_ = g.projVersion;
}

void GfxState::InitShader(Shader& s)
{
    s.mvpLoc_ = glGetUniformLocation(s.program_, "mvp");
    glUseProgram(s.program_);
    int t = glGetUniformLocation(s.program_, "texture0");
    if (t >= 0) glUniform1i(t, 0);
}

static unsigned Compile(GLenum type, const char* src)
{
    unsigned s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        std::fprintf(stderr, "[gfx] shader error: %s\n", log);
    }
    return s;
}

Shader::Shader(const char* vertex, const char* fragment)
{
    if (!vertex) vertex = kVertexShader;
    unsigned vs = Compile(GL_VERTEX_SHADER, vertex), fs = Compile(GL_FRAGMENT_SHADER, fragment);
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glBindAttribLocation(program_, 0, "vertexPosition");
    glBindAttribLocation(program_, 1, "vertexTexCoord");
    glBindAttribLocation(program_, 2, "vertexNormal");
    glBindAttribLocation(program_, 3, "vertexColor");
    glLinkProgram(program_);
    int ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[2048];
        glGetProgramInfoLog(program_, sizeof log, nullptr, log);
        std::fprintf(stderr, "[gfx] link error: %s\n", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    GfxState::InitShader(*this);
    // restore the active program
    if (g.shader) glUseProgram(g.shader->program_);
}

int Shader::Location(const char* uniform) const { return glGetUniformLocation(program_, uniform); }

void Shader::SetInt(int loc, int v)
{
    gfx::Flush();
    glUseProgram(program_);
    glUniform1i(loc, v);
    if (g.shader && g.shader != this) glUseProgram(g.shader->Program());
}
void Shader::SetFloat(int loc, float v)
{
    gfx::Flush();
    glUseProgram(program_);
    glUniform1f(loc, v);
    if (g.shader && g.shader != this) glUseProgram(g.shader->Program());
}
void Shader::SetVec2(int loc, float x, float y)
{
    gfx::Flush();
    glUseProgram(program_);
    glUniform2f(loc, x, y);
    if (g.shader && g.shader != this) glUseProgram(g.shader->Program());
}

namespace gfx {

static void SetProjection(int w, int h)
{
    // Orthographic, pixel coordinates with y down.
    float* m = g.mvp;
    for (int i = 0; i < 16; i++) m[i] = 0;
    m[0] = 2.0f / w;
    m[5] = -2.0f / h;
    m[10] = -1;
    m[12] = -1;
    m[13] = 1;
    m[15] = 1;
    g.projVersion++;
    glViewport(0, 0, w, h);
}

bool Init()
{
    glGenVertexArrays(1, &g.vao);
    glGenBuffers(1, &g.vbo);
    glBindVertexArray(g.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    const GLsizei stride = sizeof(Vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, nx));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)offsetof(Vertex, r));
    uint8_t white[4] = {255, 255, 255, 255};
    g.white = CreateTexture(1, 1, white, false);
    g.defaultShader = Shader(nullptr, kDefaultFragment);
    g.shader = &g.defaultShader;
    glUseProgram(g.defaultShader.Program());
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied alpha
    g.verts.reserve(6 * 4096);
    return true;
}

void Shutdown() {}

void BeginFrame(int width, int height)
{
    g.fbW = width > 0 ? width : 1;
    g.fbH = height > 0 ? height : 1;
    g.target = nullptr;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    SetProjection(g.fbW, g.fbH);
    NoScissor();
}

void EndFrame() { Flush(); }

void FramebufferSize(int& width, int& height)
{
    width = g.fbW;
    height = g.fbH;
}

void SetTarget(RenderTarget* target)
{
    if (target == g.target) return;
    Flush();
    g.target = target;
    if (target)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, target->Fbo);
        SetProjection(target->Width, target->Height);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        SetProjection(g.fbW, g.fbH);
    }
}

RenderTarget CreateTarget(int width, int height)
{
    RenderTarget t;
    t.Width = width;
    t.Height = height;
    glGenTextures(1, &t.Texture);
    glBindTexture(GL_TEXTURE_2D, t.Texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &t.Fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, t.Fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.Texture, 0);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, g.target ? g.target->Fbo : 0);
    glBindTexture(GL_TEXTURE_2D, g.texture ? g.texture : g.white);
    return t;
}

void DestroyTarget(RenderTarget& t)
{
    if (t.Fbo) glDeleteFramebuffers(1, &t.Fbo);
    if (t.Texture) glDeleteTextures(1, &t.Texture);
    t = RenderTarget{};
}

void Clear(Color c)
{
    Flush();
    glClearColor(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void UseShader(Shader* shader)
{
    if (!shader) shader = &g.defaultShader;
    if (shader == g.shader) return;
    Flush();
    g.shader = shader;
    glUseProgram(shader->Program());
}

Shader* CurrentShader() { return g.shader == &g.defaultShader ? nullptr : g.shader; }

void SetTexture(unsigned texture)
{
    if (texture == g.texture) return;
    Flush();
    g.texture = texture;
}

void Flush()
{
    if (g.verts.empty()) return;
    glUseProgram(g.shader->Program());
    GfxState::ApplyProjection(*g.shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g.texture ? g.texture : g.white);
    glBindVertexArray(g.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    glBufferData(GL_ARRAY_BUFFER, g.verts.size() * sizeof(Vertex), g.verts.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)g.verts.size());
    g.verts.clear();
}

void Quad(const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d)
{
    auto& v = g.verts;
    v.push_back(a); v.push_back(b); v.push_back(c);
    v.push_back(a); v.push_back(c); v.push_back(d);
    if (v.size() > 60000) Flush();
}

void Triangle(const Vertex& a, const Vertex& b, const Vertex& c)
{
    auto& v = g.verts;
    v.push_back(a); v.push_back(b); v.push_back(c);
    if (v.size() > 60000) Flush();
}

void Scissor(Rect r)
{
    Flush();
    g.scissor = true;
    g.scissorRect = r;
    int h = g.target ? g.target->Height : g.fbH;
    glEnable(GL_SCISSOR_TEST);
    int x = (int)r.x, y = (int)r.y, w = (int)r.w, hh = (int)r.h;
    if (w < 0) w = 0;
    if (hh < 0) hh = 0;
    glScissor(x, h - (y + hh), w, hh);
}

void NoScissor()
{
    Flush();
    g.scissor = false;
    glDisable(GL_SCISSOR_TEST);
}

// ---------------- shapes ----------------

static Vertex V(Vec2 p, Color c, float u = 0, float v = 0) { return {p.x, p.y, u, v, 0, 0, 1, c.r, c.g, c.b, c.a}; }

static void ShapeMode()
{
    UseShader(nullptr);
    SetTexture(0);
}

void DrawTriangle(Vec2 a, Vec2 b, Vec2 c, Color col)
{
    ShapeMode();
    Triangle(V(a, col), V(b, col), V(c, col));
}

void DrawLine(Vec2 a, Vec2 b, float thick, Color col)
{
    ShapeMode();
    Vec2 d = b - a;
    float len = d.Length();
    if (len < 1e-4f) return;
    Vec2 n = Vec2(-d.y / len, d.x / len) * (thick / 2);
    Quad(V(a + n, col), V(b + n, col), V(b - n, col), V(a - n, col));
}

void DrawRect(Rect r, Color col)
{
    ShapeMode();
    Quad(V({r.x, r.y}, col), V({r.x + r.w, r.y}, col), V({r.x + r.w, r.y + r.h}, col), V({r.x, r.y + r.h}, col));
}

void DrawRectLines(Rect r, float t, Color col)
{
    DrawRect({r.x, r.y, r.w, t}, col);
    DrawRect({r.x, r.y + r.h - t, r.w, t}, col);
    DrawRect({r.x, r.y + t, t, r.h - 2 * t}, col);
    DrawRect({r.x + r.w - t, r.y + t, t, r.h - 2 * t}, col);
}

static void CornerArc(std::vector<Vec2>& pts, Vec2 c, float rad, float startDeg, int segments)
{
    for (int i = 0; i <= segments; i++)
    {
        float a = (startDeg + 90.0f * i / segments) * 3.14159265f / 180;
        pts.push_back({c.x + std::cos(a) * rad, c.y + std::sin(a) * rad});
    }
}

static std::vector<Vec2> RoundedOutline(Rect r, float roundness, int segments, float inset)
{
    float rad = std::min(r.w, r.h) * 0.5f * std::max(0.0f, std::min(roundness, 1.0f)) - inset;
    if (rad < 0) rad = 0;
    Rect q{r.x + inset, r.y + inset, r.w - 2 * inset, r.h - 2 * inset};
    std::vector<Vec2> pts;
    if (segments < 1) segments = 4;
    CornerArc(pts, {q.x + q.w - rad, q.y + rad}, rad, 270, segments);         // top right
    CornerArc(pts, {q.x + q.w - rad, q.y + q.h - rad}, rad, 0, segments);     // bottom right
    CornerArc(pts, {q.x + rad, q.y + q.h - rad}, rad, 90, segments);          // bottom left
    CornerArc(pts, {q.x + rad, q.y + rad}, rad, 180, segments);               // top left
    return pts;
}

void DrawRectRounded(Rect r, float roundness, int segments, Color col)
{
    ShapeMode();
    auto pts = RoundedOutline(r, roundness, segments, 0);
    Vec2 c{r.x + r.w / 2, r.y + r.h / 2};
    for (size_t i = 0; i < pts.size(); i++) Triangle(V(c, col), V(pts[i], col), V(pts[(i + 1) % pts.size()], col));
}

void DrawRectRoundedLines(Rect r, float roundness, int segments, float thick, Color col)
{
    ShapeMode();
    auto outer = RoundedOutline(r, roundness, segments, 0);
    auto inner = RoundedOutline(r, roundness, segments, thick);
    for (size_t i = 0; i < outer.size(); i++)
    {
        size_t j = (i + 1) % outer.size();
        Quad(V(outer[i], col), V(outer[j], col), V(inner[j], col), V(inner[i], col));
    }
}

void DrawCircle(Vec2 c, float radius, Color col)
{
    ShapeMode();
    int n = 36;
    for (int i = 0; i < n; i++)
    {
        float a0 = 6.2831853f * i / n, a1 = 6.2831853f * (i + 1) / n;
        Triangle(V(c, col), V({c.x + std::cos(a0) * radius, c.y + std::sin(a0) * radius}, col),
                 V({c.x + std::cos(a1) * radius, c.y + std::sin(a1) * radius}, col));
    }
}

void DrawCircleLines(Vec2 c, float radius, Color col) { DrawRing(c, radius - 0.5f, radius + 0.5f, 0, 360, 48, col); }

void DrawRing(Vec2 c, float inner, float outer, float startDeg, float endDeg, int segments, Color col)
{
    ShapeMode();
    if (segments < 4) segments = 4;
    for (int i = 0; i < segments; i++)
    {
        float a0 = (startDeg + (endDeg - startDeg) * i / segments) * 3.14159265f / 180;
        float a1 = (startDeg + (endDeg - startDeg) * (i + 1) / segments) * 3.14159265f / 180;
        Vec2 o0{c.x + std::cos(a0) * outer, c.y + std::sin(a0) * outer}, o1{c.x + std::cos(a1) * outer, c.y + std::sin(a1) * outer};
        Vec2 i0{c.x + std::cos(a0) * inner, c.y + std::sin(a0) * inner}, i1{c.x + std::cos(a1) * inner, c.y + std::sin(a1) * inner};
        Quad(V(o0, col), V(o1, col), V(i1, col), V(i0, col));
    }
}

void DrawTexture(unsigned texture, Rect d, bool flipV, Color t)
{
    SetTexture(texture);
    float v0 = flipV ? 1.0f : 0.0f, v1 = flipV ? 0.0f : 1.0f;
    Quad(V({d.x, d.y}, t, 0, v0), V({d.x + d.w, d.y}, t, 1, v0), V({d.x + d.w, d.y + d.h}, t, 1, v1), V({d.x, d.y + d.h}, t, 0, v1));
}

// ---------------- textures ----------------

unsigned CreateTexture(int width, int height, const uint8_t* rgba, bool mipmaps)
{
    Flush();
    unsigned tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (mipmaps)
    {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    else glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, g.texture ? g.texture : g.white);
    return tex;
}

void DeleteTexture(unsigned texture)
{
    if (texture) glDeleteTextures(1, &texture);
}

// ---------------- text ----------------

Font LoadFont(const std::string& path, float bakedSize)
{
    Font f;
    std::ifstream in(path, std::ios::binary);
    if (!in) return f;
    std::vector<unsigned char> ttf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const int size = 1024;
    std::vector<unsigned char> alpha(size * size);
    std::vector<stbtt_bakedchar> baked(224);
    if (stbtt_BakeFontBitmap(ttf.data(), 0, bakedSize, alpha.data(), size, size, 32, 224, baked.data()) <= 0) return f;
    stbtt_fontinfo info;
    stbtt_InitFont(&info, ttf.data(), stbtt_GetFontOffsetForIndex(ttf.data(), 0));
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
    f.Ascent = ascent * stbtt_ScaleForPixelHeight(&info, bakedSize);
    std::vector<uint8_t> rgba(size * size * 4);
    for (int i = 0; i < size * size; i++)
    {
        rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = alpha[i];
    }
    f.Texture = CreateTexture(size, size, rgba.data(), false);
    f.BakedSize = bakedSize;
    f.AtlasSize = size;
    for (auto& b : baked) f.Glyphs.push_back({(float)b.x0, (float)b.y0, (float)b.x1, (float)b.y1, b.xoff, b.yoff, b.xadvance});
    return f;
}

static std::u32string Decode(const std::string& s)
{
    std::u32string out;
    for (size_t i = 0; i < s.size();)
    {
        unsigned char c = s[i];
        int n = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : 3;
        char32_t cp = n == 0 ? c : n == 1 ? (c & 0x1F) : n == 2 ? (c & 0x0F) : (c & 0x07);
        for (int k = 1; k <= n && i + k < s.size(); k++) cp = (cp << 6) | (s[i + k] & 0x3F);
        out.push_back(cp);
        i += n + 1;
    }
    return out;
}

void DrawText(const Font& f, const std::string& utf8, float x, float y, float size, Color col)
{
    if (!f.Valid()) return;
    UseShader(nullptr);
    SetTexture(f.Texture);
    float k = size / f.BakedSize;
    float pen = x, base = y + f.Ascent * k;
    float inv = 1.0f / f.AtlasSize;
    for (char32_t cp : Decode(utf8))
    {
        if (cp < 32 || cp > 255) cp = '?';
        const auto& gl = f.Glyphs[cp - 32];
        float x0 = pen + gl.xoff * k, y0 = base + gl.yoff * k;
        float x1 = x0 + (gl.x1 - gl.x0) * k, y1 = y0 + (gl.y1 - gl.y0) * k;
        Quad(V({x0, y0}, col, gl.x0 * inv, gl.y0 * inv), V({x1, y0}, col, gl.x1 * inv, gl.y0 * inv),
             V({x1, y1}, col, gl.x1 * inv, gl.y1 * inv), V({x0, y1}, col, gl.x0 * inv, gl.y1 * inv));
        pen += gl.xadvance * k;
    }
}

float MeasureText(const Font& f, const std::string& utf8, float size)
{
    if (!f.Valid()) return 0;
    float w = 0;
    for (char32_t cp : Decode(utf8))
    {
        if (cp < 32 || cp > 255) cp = '?';
        w += f.Glyphs[cp - 32].xadvance;
    }
    return w * size / f.BakedSize;
}

bool SaveScreenshot(const std::string& path, int width, int height)
{
    Flush();
    std::vector<uint8_t> px(width * height * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    std::vector<uint8_t> flipped(px.size());
    for (int y = 0; y < height; y++)
        std::memcpy(&flipped[y * width * 4], &px[(height - 1 - y) * width * 4], width * 4);
    for (int i = 0; i < width * height; i++) flipped[i * 4 + 3] = 255;
    glBindFramebuffer(GL_FRAMEBUFFER, g.target ? g.target->Fbo : 0);
    return stbi_write_png(path.c_str(), width, height, 4, flipped.data(), width * 4) != 0;
}

} // namespace gfx
} // namespace scratch
