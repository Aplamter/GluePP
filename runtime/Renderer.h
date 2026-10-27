// Draws the stage the way Scratch's renderer does: a high-quality pen layer (pen lines and
// stamps, premultiplied alpha) on top of the backdrop, with visible sprites above it.
#pragma once
#include "Gfx.h"

namespace scratch {

class Engine;
class Target;
class SpriteBase;
struct Costume;
enum class RotationStyle;

class Renderer
{
public:
    explicit Renderer(Engine& engine) : engine_(engine) {}
    void Init(int width, int height);
    /// Keeps the pen layer at the on-screen stage resolution ("high quality pen"), keeping its drawing.
    void ResizePen(int width, int height);

    void ClearPen();
    /// One pen line with round caps, anti-aliased like Scratch's pen shader.
    void PenLine(const SpriteBase& s, double x0, double y0, double x1, double y1);
    void Stamp(const SpriteBase& s);
    /// "touching mouse-pointer?": is the point inside a non-transparent pixel of the sprite?
    bool TouchingPoint(const SpriteBase& s, double x, double y) const;

    /// Draws the frame: backdrop, pen layer, sprites, then overlays.
    void Present(Rect stageRect, int fbWidth, int fbHeight);

private:
    Engine& engine_;
    RenderTarget pen_;
    float penQ_ = 1; // pen-layer pixels per stage unit
    Shader lineShader_, spriteShader_;
    int locEffects_ = -1, locColor_ = -1, locBrightness_ = -1, locGhost_ = -1, locPixelate_ = -1, locSkinSize_ = -1;
    // Last uniform values sent, so batches only break when effects actually change.
    int uEffects_ = -1;
    float uColor_ = -1, uBrightness_ = 0, uGhost_ = 0, uPixelate_ = 0, uSkinW_ = 0, uSkinH_ = 0;
    bool penActive_ = false;

    void BeginPenTarget();
    void EndPenTarget();
    void SetUniforms(const Target& t, const Costume& c);
    void DrawTarget(const Target& t, const Costume& c, double x, double y, double direction, double size,
                    RotationStyle style, float offX, float offY, float q);
};

} // namespace scratch
