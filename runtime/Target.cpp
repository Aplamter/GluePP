#include "Target.h"

#include <cmath>

#include "Engine.h"

namespace scratch {

using namespace ops;

PenParam ParsePenParam(const String& s)
{
    String l = s.Lower();
    if (l == u"saturation") return PenParam::Saturation;
    if (l == u"brightness") return PenParam::Brightness;
    if (l == u"transparency") return PenParam::Transparency;
    return PenParam::Color;
}

// ---------------- looks ----------------

void Target::SetCostumeIndex(double index)
{
    index = Round(index);
    if (std::isinf(index) || index != index) index = 0;
    CurrentCostume = (int)WrapClamp(index, 0, (double)Costumes->Items.size() - 1);
    OnVisualChange();
}

void Target::SwitchCostume(const Value& c)
{
    if (c.IsNumber()) { SetCostumeIndex(c.RawNumber() - 1); return; }
    String s = c.Str();
    int found = Costumes->Find(s);
    if (found >= 0) { SetCostumeIndex(found); return; }
    if (s == u"next costume" || s == u"next backdrop") SetCostumeIndex(CurrentCostume + 1);
    else if (s == u"previous costume" || s == u"previous backdrop") SetCostumeIndex(CurrentCostume - 1);
    else
    {
        double n = c.JsNumber();
        if (n == n && !c.IsWhiteSpace()) SetCostumeIndex(n - 1);
    }
}

void Target::SetEffect(Effect e, double v)
{
    if (e == Effect::Ghost) v = Clamp(v, 0, 100);
    else if (e == Effect::Brightness) v = Clamp(v, -100, 100);
    Effects[(int)e] = v;
    OnVisualChange();
}

void Target::ClearEffects()
{
    for (double& e : Effects) e = 0;
    OnVisualChange();
}

// ---------------- sound ----------------

int Target::SoundIndex(const Value& sound) const
{
    int len = (int)Sounds->Items.size();
    if (len == 0) return -1;
    String name = sound.Str();
    for (int i = 0; i < len; i++)
        if (Sounds->Items[i].Name == name) return i;
    // parseInt semantics
    auto t = name.view();
    size_t a = 0, b = t.size();
    while (a < b && Js::IsSpace(t[a])) a++;
    while (b > a && Js::IsSpace(t[b - 1])) b--;
    size_t j = a;
    bool neg = false;
    if (j < b && (t[j] == '-' || t[j] == '+')) { neg = t[j] == '-'; j++; }
    long long v = 0;
    int digits = 0;
    while (j < b && t[j] >= '0' && t[j] <= '9' && digits < 15) { v = v * 10 + (t[j] - '0'); j++; digits++; }
    if (digits == 0) return -1;
    return (int)WrapClamp((double)((neg ? -v : v) - 1), 0, len - 1);
}

void Target::StartSound(const Value& sound)
{
    int i = SoundIndex(sound);
    if (i >= 0) engine->Audio.Play(*this, i);
}

void Target::SetVolume(double v)
{
    Volume = Clamp(v, 0, 100);
    engine->Audio.Sync(*this);
}

void Target::SetSoundEffect(SoundEffect e, double v)
{
    // The project disables TurboWarp's "misc limits", which widens the pitch range.
    if (e == SoundEffect::Pitch) SoundPitch = Clamp(v, -1000, 1000);
    else SoundPan = Clamp(v, -100, 100);
    engine->Audio.Sync(*this);
}

void Target::ClearSoundEffects()
{
    SoundPitch = 0;
    SoundPan = 0;
    engine->Audio.Sync(*this);
}

void Target::StopAllSounds() { engine->Audio.StopAll(); }

// ---------------- sensing / events / control ----------------

double Target::MouseX() const { return engine->Input.MouseX; }
double Target::MouseY() const { return engine->Input.MouseY; }
bool Target::MouseDown() const { return engine->Input.MouseDown; }
bool Target::RightMouseDown() const { return engine->Input.RightMouseDown; }
double Target::LookDeltaX() const { return engine->Input.LookDeltaX; }
double Target::LookDeltaY() const { return engine->Input.LookDeltaY; }
bool Target::KeyPressed(const Value& key) const { return engine->Input.IsKeyDown(key); }
double Target::Timer() const { return engine->Timer(); }
void Target::ResetTimer() { engine->ResetTimer(); }
void Target::Broadcast(const String& name) { engine->Broadcast(name); }
void Target::StopOtherScriptsInSprite() { engine->StopOtherScripts(this); }
void Target::StopAll() { engine->StopAll(); }
void Target::PenClear() { engine->renderer.ClearPen(); }
void Target::ShowList(ScratchList& list) { engine->monitors.SetVisible(list, true); }
void Target::HideList(ScratchList& list) { engine->monitors.SetVisible(list, false); }

void Target::BusyWait(double seconds)
{
    double end = engine->Now() + std::max(0.0, seconds);
    while (engine->Now() < end) {}
}

Script Target::Wait(double seconds)
{
    double end = engine->Now() + std::max(0.0, seconds);
    engine->RequestRedraw();
    co_yield NextFrame;
    while (engine->Now() < end) co_yield NextFrame;
}

// ---------------- sprites ----------------

void SpriteBase::OnVisualChange()
{
    if (Visible) engine->RequestRedraw();
}

void SpriteBase::GoTo(double x, double y)
{
    double oldX = X, oldY = Y;
    X = x; // the project turns TurboWarp's fencing off, so sprites aren't kept on stage
    Y = y;
    if (Visible) engine->RequestRedraw();
    if (PenIsDown) engine->renderer.PenLine(*this, oldX, oldY, X, Y);
}

void SpriteBase::PointInDirection(double d)
{
    if (!std::isfinite(d)) return;
    Direction = WrapClamp(d, -179, 180);
    OnVisualChange();
}

void SpriteBase::SetSize(double s)
{
    Size = 100 * JsMax(0.0, s / 100);
    OnVisualChange();
}

void SpriteBase::Show()
{
    Visible = true;
    engine->RequestRedraw();
}

void SpriteBase::Hide()
{
    if (Visible) engine->RequestRedraw();
    Visible = false;
}

void SpriteBase::PenDown()
{
    PenIsDown = true;
    engine->renderer.PenLine(*this, X, Y, X, Y);
}

void SpriteBase::Stamp() { engine->renderer.Stamp(*this); }

void SpriteBase::SetPenColor(const Value& color)
{
    auto c = ColorUtil::ToRgb(color);
    double h, s, v;
    ColorUtil::RgbToHsv(c.r, c.g, c.b, h, s, v);
    PenColor = h / 360 * 100;
    PenSaturation = s * 100;
    PenBrightness = v * 100;
    PenTransparency = c.hasAlpha ? 100 * (1 - c.a / 255.0) : 0;
    UpdatePenColor();
}

void SpriteBase::SetOrChangePenParam(PenParam p, double v, bool change)
{
    switch (p)
    {
    case PenParam::Color: PenColor = WrapClamp(v + (change ? PenColor : 0), 0, 100); break;
    case PenParam::Saturation: PenSaturation = Clamp(v + (change ? PenSaturation : 0), 0, 100); break;
    case PenParam::Brightness: PenBrightness = Clamp(v + (change ? PenBrightness : 0), 0, 100); break;
    case PenParam::Transparency: PenTransparency = Clamp(v + (change ? PenTransparency : 0), 0, 100); break;
    }
    UpdatePenColor();
}

void SpriteBase::UpdatePenColor()
{
    int r, g, b;
    ColorUtil::HsvToRgb(PenColor * 360 / 100, PenSaturation / 100, PenBrightness / 100, r, g, b);
    PenR = r / 255.0f;
    PenG = g / 255.0f;
    PenB = b / 255.0f;
    PenA = (float)(1 - PenTransparency / 100);
}

bool SpriteBase::TouchingMouse() const { return engine->renderer.TouchingPoint(*this, engine->Input.MouseX, engine->Input.MouseY); }

void SpriteBase::CreateClone()
{
    SpriteBase* clone = CloneSelf(); // copies everything, including this sprite's own lists
    clone->IsClone = true;
    clone->SoundVoices = nullptr;
    clone->Disposed = false;
    engine->AddClone(this, clone);
}

void SpriteBase::DeleteThisClone()
{
    if (!IsClone) return;
    engine->RemoveClone(this);
    throw StopScript{};
}

// ---------------- colors ----------------

namespace ColorUtil {

Rgba ToRgb(const Value& v)
{
    if (v.IsString() && v.RawString().StartsWith(u"#"))
    {
        String h = v.RawString().Substr(1, v.RawString().size() - 1);
        std::u16string hex(h.view());
        if (hex.size() == 3) hex = {hex[0], hex[0], hex[1], hex[1], hex[2], hex[2]};
        auto hv = [](char16_t c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
        if (hex.size() == 6)
        {
            int d[6];
            bool ok = true;
            for (int i = 0; i < 6; i++) { d[i] = hv(hex[i]); if (d[i] < 0) ok = false; }
            if (ok) return {d[0] * 16 + d[1], d[2] * 16 + d[3], d[4] * 16 + d[5], 255, false};
        }
        return {0, 0, 0, 255, true};
    }
    double d = v.Num();
    if (!std::isfinite(d)) d = 0;
    // JavaScript ToInt32
    double m = std::fmod(std::trunc(d), 4294967296.0);
    if (m < 0) m += 4294967296.0;
    uint32_t n = (uint32_t)m;
    int a = (n >> 24) & 0xFF;
    return {(int)((n >> 16) & 0xFF), (int)((n >> 8) & 0xFF), (int)(n & 0xFF), a > 0 ? a : 255, true};
}

void RgbToHsv(int ri, int gi, int bi, double& h, double& s, double& v)
{
    double r = ri / 255.0, g = gi / 255.0, b = bi / 255.0;
    double x = std::min(std::min(r, g), b);
    v = std::max(std::max(r, g), b);
    h = 0;
    s = 0;
    if (x != v)
    {
        double f = r == x ? g - b : (g == x ? b - r : r - g);
        double i = r == x ? 3 : (g == x ? 5 : 1);
        h = std::fmod((i - f / (v - x)) * 60, 360);
        s = (v - x) / v;
    }
}

void HsvToRgb(double h, double s, double v, int& ri, int& gi, int& bi)
{
    h = std::fmod(h, 360);
    if (h < 0) h += 360;
    s = std::max(0.0, std::min(s, 1.0));
    v = std::max(0.0, std::min(v, 1.0));
    int i = (int)std::floor(h / 60);
    double f = h / 60 - i;
    double p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    double r, g, b;
    switch (i)
    {
    default: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
    }
    ri = (int)std::floor(r * 255);
    gi = (int)std::floor(g * 255);
    bi = (int)std::floor(b * 255);
}

} // namespace ColorUtil
} // namespace scratch
