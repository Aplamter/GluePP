#include "Renderer.h"

#include <algorithm>
#include <cmath>

#include "Engine.h"
#include "Shaders.h"

namespace scratch {

void Renderer::Init(int width, int height)
{
    lineShader_ = Shader(nullptr, shaders::LineFragment);
    spriteShader_ = Shader(nullptr, shaders::SpriteFragment);
    locEffects_ = spriteShader_.Location("u_effects");
    locColor_ = spriteShader_.Location("u_color");
    locBrightness_ = spriteShader_.Location("u_brightness");
    locGhost_ = spriteShader_.Location("u_ghost");
    locPixelate_ = spriteShader_.Location("u_pixelate");
    locSkinSize_ = spriteShader_.Location("u_skinSize");
    ResizePen(width, height);
}

void Renderer::ResizePen(int width, int height)
{
    width = std::max(1, width);
    height = std::max(1, height);
    if (width == pen_.Width && height == pen_.Height) return;
    EndPenTarget();
    RenderTarget fresh = gfx::CreateTarget(width, height);
    if (pen_.Width > 0)
    {
        RenderTarget* prev = nullptr;
        gfx::SetTarget(&fresh);
        Shader* prevShader = gfx::CurrentShader();
        gfx::UseShader(&spriteShader_);
        if (uEffects_ != 0) { spriteShader_.SetInt(locEffects_, 0); uEffects_ = 0; }
        gfx::DrawTexture(pen_.Texture, {0, 0, (float)width, (float)height}, true);
        gfx::Flush();
        gfx::SetTarget(prev);
        gfx::UseShader(prevShader);
        gfx::DestroyTarget(pen_);
    }
    pen_ = fresh;
    penQ_ = width / 480.0f;
}

void Renderer::BeginPenTarget()
{
    if (penActive_) return;
    gfx::SetTarget(&pen_);
    penActive_ = true;
}

void Renderer::EndPenTarget()
{
    if (!penActive_) return;
    gfx::SetTarget(nullptr);
    penActive_ = false;
}

// ---------------- pen ----------------

void Renderer::ClearPen()
{
    engine_.RequestRedraw();
    BeginPenTarget();
    gfx::Clear({0, 0, 0, 0});
}

void Renderer::PenLine(const SpriteBase& s, double x0, double y0, double x1, double y1)
{
    engine_.RequestRedraw();
    BeginPenTarget();
    gfx::UseShader(&lineShader_);
    gfx::SetTexture(0);

    double diameter = s.PenSize;
    double off = (diameter == 1 || diameter == 3) ? 0.5 : 0;
    float q = penQ_;
    float ax = (float)((x0 + off + 240) * q), ay = (float)((180 - (y0 + off)) * q);
    float bx = (float)((x1 + off + 240) * q), by = (float)((180 - (y1 + off)) * q);
    float dx = bx - ax, dy = by - ay;
    float len = std::sqrt(dx * dx + dy * dy);
    float thick = (float)(diameter * q);
    float r = thick * 0.5f + 1.4142135623730951f;
    float ux, uy;
    if (len < 1e-3f) { ux = 1; uy = 0; }
    else { ux = dx / len; uy = dy / len; }
    float nx = -uy, ny = ux;

    uint8_t cr = (uint8_t)std::lround(s.PenR * 255), cg = (uint8_t)std::lround(s.PenG * 255), cb = (uint8_t)std::lround(s.PenB * 255);
    float alpha = s.PenA;
    auto V = [&](float x, float y, float along, float across) {
        return gfx::Vertex{x, y, along, across, thick, len, alpha, cr, cg, cb, 255};
    };
    gfx::Quad(V(ax - ux * r - nx * r, ay - uy * r - ny * r, -r, -r), V(ax - ux * r + nx * r, ay - uy * r + ny * r, -r, r),
              V(bx + ux * r + nx * r, by + uy * r + ny * r, len + r, r), V(bx + ux * r - nx * r, by + uy * r - ny * r, len + r, -r));
}

void Renderer::Stamp(const SpriteBase& s)
{
    engine_.RequestRedraw();
    BeginPenTarget();
    gfx::UseShader(&spriteShader_);
    DrawTarget(s, s.GetCostume(), s.X, s.Y, s.Direction, s.Size, s.Rotation, 0, 0, penQ_);
}

// ---------------- sprites ----------------

static int EffectMask(const double* fx)
{
    int mask = 0;
    for (int i = 0; i < 7; i++)
        if (fx[i] != 0 && fx[i] == fx[i]) mask |= 1 << i;
    return mask;
}

void Renderer::SetUniforms(const Target& t, const Costume& c)
{
    const double* fx = t.Effects;
    int mask = EffectMask(fx);
    float color = (float)std::fmod(fx[(int)Effect::Color] / 200, 1.0);
    float bright = (float)(std::max(-100.0, std::min(fx[(int)Effect::Brightness], 100.0)) / 100);
    float ghost = (float)(1 - std::max(0.0, std::min(fx[(int)Effect::Ghost], 100.0)) / 100);
    float pix = (float)(std::fabs(fx[(int)Effect::Pixelate]) / 10);
    float sw = (float)c.Width, sh = (float)c.Height;
    // Only values of enabled effects matter; skipping the rest keeps text glyph stamps in one batch.
    bool same = mask == uEffects_ && ((mask & (1 << (int)Effect::Color)) == 0 || color == uColor_) &&
                ((mask & (1 << (int)Effect::Brightness)) == 0 || bright == uBrightness_) &&
                ((mask & (1 << (int)Effect::Ghost)) == 0 || ghost == uGhost_) &&
                ((mask & (1 << (int)Effect::Pixelate)) == 0 || (pix == uPixelate_ && sw == uSkinW_ && sh == uSkinH_));
    if (same) return;
    uEffects_ = mask; uColor_ = color; uBrightness_ = bright; uGhost_ = ghost; uPixelate_ = pix; uSkinW_ = sw; uSkinH_ = sh;
    spriteShader_.SetInt(locEffects_, mask);
    spriteShader_.SetFloat(locColor_, color);
    spriteShader_.SetFloat(locBrightness_, bright);
    spriteShader_.SetFloat(locGhost_, ghost);
    spriteShader_.SetFloat(locPixelate_, pix);
    spriteShader_.SetVec2(locSkinSize_, sw, sh);
}

void Renderer::DrawTarget(const Target& t, const Costume& c, double x, double y, double direction, double size,
                          RotationStyle style, float offX, float offY, float q)
{
    SetUniforms(t, c);
    double angle = 0, flip = 1;
    if (style == RotationStyle::AllAround) angle = (direction - 90) * kPi / 180;
    else if (style == RotationStyle::LeftRight && direction < 0) flip = -1;
    double k = size / 100 * q;
    double cs = std::cos(angle), sn = std::sin(angle);
    double px = offX + (x + 240) * q, py = offY + (180 - y) * q;
    double l = -c.CenterX, r = c.Width - c.CenterX, top = -c.CenterY, bot = c.Height - c.CenterY;

    gfx::SetTexture(c.Texture);
    auto Corner = [&](double lx, double ly, float u, float v) {
        double sx = lx * k * flip, sy = ly * k;
        return gfx::Vertex{(float)(px + sx * cs - sy * sn), (float)(py + sx * sn + sy * cs), u, v, 0, 0, 1, 255, 255, 255, 255};
    };
    gfx::Quad(Corner(l, top, 0, 0), Corner(l, bot, 0, 1), Corner(r, bot, 1, 1), Corner(r, top, 1, 0));
}

bool Renderer::TouchingPoint(const SpriteBase& s, double x, double y) const
{
    const Costume& c = s.GetCostume();
    double k = s.Size / 100;
    if (k == 0) return false;
    double angle = 0, flip = 1;
    if (s.Rotation == RotationStyle::AllAround) angle = (s.Direction - 90) * kPi / 180;
    else if (s.Rotation == RotationStyle::LeftRight && s.Direction < 0) flip = -1;
    double dx = x - s.X, dy = -(y - s.Y);
    double cs = std::cos(angle), sn = std::sin(angle);
    double lx = (dx * cs + dy * sn) / k * flip;
    double ly = (-dx * sn + dy * cs) / k;
    return c.IsOpaqueAt(lx + c.CenterX, ly + c.CenterY);
}

// ---------------- frame ----------------

void Renderer::Present(Rect stageRect, int fbWidth, int fbHeight)
{
    EndPenTarget();
    gfx::Flush();
    ResizePen((int)stageRect.w, (int)stageRect.h);

    gfx::BeginFrame(fbWidth, fbHeight);
    gfx::Clear(Color::Black());
    gfx::DrawRect(stageRect, Color::White());
    float q = stageRect.w / 480.0f;

    gfx::UseShader(&spriteShader_);
    auto& stage = *engine_.Stage;
    DrawTarget(stage, stage.GetCostume(), 0, 0, 90, 100, RotationStyle::AllAround, stageRect.x, stageRect.y, q);

    // The pen layer is already premultiplied: draw it with no effects.
    if (uEffects_ != 0) { spriteShader_.SetInt(locEffects_, 0); uEffects_ = 0; }
    gfx::DrawTexture(pen_.Texture, stageRect, true);

    for (SpriteBase* s : engine_.Sprites)
        if (s->Visible) DrawTarget(*s, s->GetCostume(), s->X, s->Y, s->Direction, s->Size, s->Rotation, stageRect.x, stageRect.y, q);
    gfx::Flush();
    gfx::UseShader(nullptr);
}

} // namespace scratch
