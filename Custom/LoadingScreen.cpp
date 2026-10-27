// Hand-written: the loading screen shown while Glue3D starts (editor and built games).
//
// It uses Glue3D's own artwork (the clouds, the light beam and the logo letters from the start menu),
// loaded first and straight from the asset files, plus a progress bar for the costumes and sounds.
#include "LoadingScreen.h"

#include <algorithm>
#include <cmath>
#include <map>

#include <nlohmann/json.hpp>

#include "Platform.h"
#include "stb_image.h"

namespace Glue3D {

using namespace scratch;

namespace {

struct Art
{
    unsigned Texture = 0;
    float Width = 0, Height = 0; // stage units
};

/// The few costumes the loading screen needs, by name (loaded once).
std::map<std::string, Art>& Artwork(const std::string& assetDir)
{
    static std::map<std::string, Art> art;
    static bool loaded = false;
    if (loaded) return art;
    loaded = true;
    std::string text;
    if (!ReadFile(PathJoin(assetDir, "manifest.json"), text)) return art;
    try
    {
        auto manifest = nlohmann::json::parse(text);
        static const char* wanted[] = {"_engine.clouds", "_engine.beam", "_engine.g", "_engine.l", "_engine.u", "_engine.e3d"};
        for (auto& t : manifest["targets"])
            for (auto& c : t["costumes"])
            {
                std::string name = c.value("name", "");
                if (art.count(name) || std::find(std::begin(wanted), std::end(wanted), name) == std::end(wanted)) continue;
                std::string bytes;
                if (!ReadFile(PathJoin(assetDir, c.value("file", "")), bytes)) continue;
                int w, h, n;
                unsigned char* px = stbi_load_from_memory((const unsigned char*)bytes.data(), (int)bytes.size(), &w, &h, &n, 4);
                if (!px) continue;
                art[name] = {gfx::CreateTexture(w, h, px, true), (float)c.value("width", 0.0), (float)c.value("height", 0.0)};
                stbi_image_free(px);
            }
    }
    catch (...)
    {
        // no artwork: the progress bar still shows
    }
    return art;
}

/// A costume centered at (x, y) stage units, at size percent, tinted.
void DrawArt(const std::map<std::string, Art>& art, const char* name, Rect stage, float x, float y, float size, Color tint)
{
    auto it = art.find(name);
    if (it == art.end()) return;
    float k = stage.w / 480.0f, s = size / 100.0f;
    float w = it->second.Width * s * k, h = it->second.Height * s * k;
    float cx = stage.x + (x + 240) * k, cy = stage.y + (180 - y) * k;
    gfx::DrawTexture(it->second.Texture, {cx - w / 2, cy - h / 2, w, h}, false, tint);
}

} // namespace

void DrawLoadingScreen(Engine& engine, Rect stage, float progress, const std::string& what)
{
    const auto& art = Artwork(engine.AssetDir);
    float k = stage.w / 480.0f;
    double t = engine.Now();
    gfx::Scissor(stage);

    // Night sky: the start menu's clouds, darkened, drifting slowly.
    DrawArt(art, "_engine.clouds", stage, (float)std::sin(t * 0.3) * 6, 0, 135, {70, 82, 105, 255});
    gfx::DrawRect(stage, {8, 10, 16, 120});

    // The light beam and the Glue3D logo, bobbing gently (the same layout as the start menu).
    float bob = 1.5f * (float)std::sin(t * 1.6);
    float pulse = 0.75f + 0.25f * (float)std::sin(t * 2.4);
    DrawArt(art, "_engine.beam", stage, 0, 22, 125, {255, 255, 255, (uint8_t)(150 * pulse)});
    DrawArt(art, "_engine.g", stage, -48, 32 + bob, 24, Color::White());
    DrawArt(art, "_engine.l", stage, -14, 28 + bob, 24, Color::White());
    DrawArt(art, "_engine.u", stage, 14, 28 + bob, 24, Color::White());
    DrawArt(art, "_engine.e3d", stage, 50, 20 + bob, 26, Color::White());

    // Progress bar.
    float barW = 220 * k, barH = 7 * k;
    Rect bar{stage.x + (stage.w - barW) / 2, stage.y + stage.h / 2 + 62 * k, barW, barH};
    gfx::DrawRectRounded({bar.x - 2 * k, bar.y - 2 * k, bar.w + 4 * k, bar.h + 4 * k}, 1, 10, {255, 255, 255, 28});
    gfx::DrawRectRounded(bar, 1, 10, {22, 26, 36, 255});
    float p = std::clamp(progress, 0.0f, 1.0f);
    if (p > 0)
    {
        Rect fill{bar.x, bar.y, std::max(bar.h, bar.w * p), bar.h};
        gfx::DrawRectRounded(fill, 1, 10, {56, 190, 236, 255});
        // a soft highlight sweeping along the filled part
        float sweep = (float)std::fmod(t * 0.8, 1.0);
        float hx = fill.x + fill.w * sweep;
        gfx::DrawRectRounded({std::max(fill.x, hx - 12 * k), fill.y, std::min(24 * k, fill.x + fill.w - std::max(fill.x, hx - 12 * k)), fill.h},
                             1, 8, {255, 255, 255, 60});
    }

    // What's loading, and the percentage.
    auto& mon = engine.monitors;
    float size = 8.5f * k;
    std::string label = what == "ready" ? "starting glue3d" : "loading " + what;
    std::string percent = std::to_string((int)std::lround(p * 100)) + "%";
    Color dim{150, 165, 190, 255};
    mon.Text(label, bar.x, bar.y + bar.h + 6 * k, size, dim);
    mon.Text(percent, bar.x + bar.w - mon.Measure(percent, size), bar.y + bar.h + 6 * k, size, dim);

    // Footer.
    std::string footer = "glue3d  -  c++ / opengl edition";
    float fs = 7 * k;
    mon.Text(footer, stage.x + (stage.w - mon.Measure(footer, fs)) / 2, stage.y + stage.h - 18 * k, fs, {90, 100, 120, 255});
    gfx::NoScissor();
}

void DrawGameLoadingScreen(const std::string& imagePath)
{
    gfx::Clear(Color::Black());
    if (imagePath.empty()) return;
    static unsigned texture = 0;
    static int w = 0, h = 0;
    static bool tried = false;
    if (!tried)
    {
        tried = true;
        std::string bytes;
        int n;
        if (ReadFile(imagePath, bytes))
            if (unsigned char* px = stbi_load_from_memory((const unsigned char*)bytes.data(), (int)bytes.size(), &w, &h, &n, 4))
            {
                texture = gfx::CreateTexture(w, h, px, true);
                stbi_image_free(px);
            }
    }
    if (!texture || w <= 0 || h <= 0) return;
    // The whole picture, as large as fits the window, keeping its shape (black bars where it doesn't fill).
    int fw = 0, fh = 0;
    gfx::FramebufferSize(fw, fh);
    float scale = std::min((float)fw / w, (float)fh / h);
    float dw = w * scale, dh = h * scale;
    gfx::DrawTexture(texture, {(fw - dw) / 2, (fh - dh) / 2, dw, dh}, false);
}

} // namespace Glue3D
