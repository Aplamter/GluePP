// A small 2D drawing layer on OpenGL 3.3: batched textured triangles, render targets, scissor,
// shapes (lines, rectangles, circles) and text. Coordinates are framebuffer pixels, y pointing down.
// Everything is drawn with premultiplied alpha, like Scratch's renderer.
#pragma once
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace scratch {

struct Vec2
{
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator-() const { return {-x, -y}; }
    Vec2 operator*(float k) const { return {x * k, y * k}; }
    float LengthSquared() const { return x * x + y * y; }
    float Length() const { return std::sqrt(x * x + y * y); }
    Vec2 Normalized() const { float l = Length(); return l > 0 ? Vec2(x / l, y / l) : Vec2(); }
    static float Dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
    static float Distance(Vec2 a, Vec2 b) { return (a - b).Length(); }
};

struct Vec3
{
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator*(float k) const { return {x * k, y * k, z * k}; }
};

struct Rect
{
    float x = 0, y = 0, w = 0, h = 0;
};

struct Color
{
    uint8_t r = 0, g = 0, b = 0, a = 255;
    static constexpr Color White() { return {255, 255, 255, 255}; }
    static constexpr Color Black() { return {0, 0, 0, 255}; }
};

class Shader
{
public:
    Shader() = default;
    Shader(const char* vertex, const char* fragment);
    unsigned Program() const { return program_; }
    int Location(const char* uniform) const;
    // Setting a uniform draws whatever is batched first, so earlier shapes keep the old value.
    void SetInt(int loc, int v);
    void SetFloat(int loc, float v);
    void SetVec2(int loc, float x, float y);

private:
    unsigned program_ = 0;
    friend struct GfxState;
    int mvpLoc_ = -1;
    int projVersion_ = -1;
};

struct RenderTarget
{
    unsigned Fbo = 0, Texture = 0;
    int Width = 0, Height = 0;
};

struct Font
{
    unsigned Texture = 0;
    float BakedSize = 0, Ascent = 0;
    struct Glyph { float x0, y0, x1, y1, xoff, yoff, xadvance; };
    std::vector<Glyph> Glyphs; // code points 32..255
    int AtlasSize = 0;
    bool Valid() const { return Texture != 0; }
};

namespace gfx {

bool Init();
void Shutdown();

/// Starts drawing to the window (framebuffer size in pixels).
void BeginFrame(int width, int height);
void EndFrame();

void SetTarget(RenderTarget* target); // nullptr = the window
/// Size of the window's drawing area (from the last BeginFrame).
void FramebufferSize(int& width, int& height);
RenderTarget CreateTarget(int width, int height);
void DestroyTarget(RenderTarget& t);
void Clear(Color c);

/// nullptr = the default shader (texture x vertex color, premultiplied).
void UseShader(Shader* shader);
Shader* CurrentShader();
void SetTexture(unsigned texture); // 0 = plain white
void Flush();

struct Vertex
{
    float x, y, u, v, nx, ny, nz;
    uint8_t r, g, b, a;
};
/// Two triangles (a, b, c) (a, c, d).
void Quad(const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d);
void Triangle(const Vertex& a, const Vertex& b, const Vertex& c);

void Scissor(Rect r);
void NoScissor();

// ---- shapes (default shader, solid colors) ----
void DrawTriangle(Vec2 a, Vec2 b, Vec2 c, Color col);
void DrawLine(Vec2 a, Vec2 b, float thick, Color col);
void DrawRect(Rect r, Color col);
void DrawRectLines(Rect r, float thick, Color col);
/// roundness 0..1 = corner radius as a fraction of half the shorter side (like raylib).
void DrawRectRounded(Rect r, float roundness, int segments, Color col);
void DrawRectRoundedLines(Rect r, float roundness, int segments, float thick, Color col);
void DrawCircle(Vec2 c, float radius, Color col);
void DrawCircleLines(Vec2 c, float radius, Color col);
void DrawRing(Vec2 c, float inner, float outer, float startDeg, float endDeg, int segments, Color col);
/// A texture into a rectangle; flipV for render-target textures.
void DrawTexture(unsigned texture, Rect dest, bool flipV, Color tint = Color::White());

// ---- textures ----
/// RGBA8 pixels, premultiplied or not as given; mipmaps make scaled-down sprites smooth.
unsigned CreateTexture(int width, int height, const uint8_t* rgba, bool mipmaps);
void DeleteTexture(unsigned texture);

// ---- text ----
Font LoadFont(const std::string& path, float bakedSize);
/// (x, y) is the top-left of the text, like raylib's DrawTextEx.
void DrawText(const Font& f, const std::string& utf8, float x, float y, float size, Color col);
float MeasureText(const Font& f, const std::string& utf8, float size);

// ---- screenshots ----
bool SaveScreenshot(const std::string& path, int width, int height);

} // namespace gfx
} // namespace scratch
