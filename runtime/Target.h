// The stage and sprites: costumes, effects, sounds, motion, pen, and the blocks every target has.
#pragma once
#include <memory>
#include <vector>

#include "Assets.h"
#include "Ops.h"
#include "Script.h"
#include "ScratchList.h"
#include "Value.h"

namespace scratch {

class Engine;
class Target;

enum class Effect { Color, Fisheye, Whirl, Pixelate, Mosaic, Brightness, Ghost };
enum class PenParam { Color, Saturation, Brightness, Transparency };
enum class SoundEffect { Pitch, Pan };
enum class RotationStyle { AllAround, LeftRight, DontRotate };
enum class HatKind { GreenFlag, Broadcast, KeyPressed, CloneStart };

PenParam ParsePenParam(const String& s);

/// An event script ("when ..."): which event starts it and the code to run.
struct Hat
{
    HatKind Kind;
    String Key;
    Script (*Start)(Target*);
};

class Target
{
public:
    Engine* engine = nullptr;
    std::shared_ptr<CostumeSet> Costumes = std::make_shared<CostumeSet>();
    std::shared_ptr<SoundSet> Sounds = std::make_shared<SoundSet>();
    int CurrentCostume = 0;
    double Effects[7] = {0, 0, 0, 0, 0, 0, 0};
    double Volume = 100;
    double SoundPitch = 0, SoundPan = 0;
    void* SoundVoices = nullptr; // per-target voices, managed by the audio engine
    bool Disposed = false;

    virtual ~Target() = default;
    virtual bool IsStage() const = 0;
    virtual const std::vector<Hat>& Hats() const = 0;
    Engine& GetEngine() const { return *engine; }

    // ---------- looks ----------
    const Costume& GetCostume() const { return Costumes->Items[CurrentCostume]; }
    double CostumeNumber() const { return CurrentCostume + 1; }
    String CostumeName() const { return GetCostume().Name; }
    void SetCostumeIndex(double index);
    /// "switch costume to": by name first, then "next/previous costume", then by number.
    void SwitchCostume(const Value& c);
    void SetEffect(Effect e, double v);
    void ChangeEffect(Effect e, double v) { SetEffect(e, Effects[(int)e] + v); }
    void ClearEffects();

    // ---------- sound ----------
    void StartSound(const Value& sound);
    void SetVolume(double v);
    void ChangeVolume(double v) { SetVolume(Volume + v); }
    void SetSoundEffect(SoundEffect e, double v);
    void ChangeSoundEffect(SoundEffect e, double v) { SetSoundEffect(e, (e == SoundEffect::Pitch ? SoundPitch : SoundPan) + v); }
    void ClearSoundEffects();
    void StopAllSounds();

    // ---------- sensing / events / control shared by all targets ----------
    double MouseX() const;
    double MouseY() const;
    bool MouseDown() const;
    bool RightMouseDown() const;
    double LookDeltaX() const;
    double LookDeltaY() const;
    bool KeyPressed(const Value& key) const;
    double Timer() const;
    void ResetTimer();
    void Broadcast(const String& name);
    void StopOtherScriptsInSprite();
    void StopAll();
    void PenClear();
    void ShowList(ScratchList& list);
    void HideList(ScratchList& list);

    /// "wait" inside a block that runs without screen refresh: Scratch blocks until the time passes.
    void BusyWait(double seconds);
    /// "wait (seconds)".
    Script Wait(double seconds);

protected:
    virtual void OnVisualChange() {}
    int SoundIndex(const Value& sound) const;
};

class StageBase : public Target
{
public:
    bool IsStage() const override { return true; }
};

class SpriteBase : public Target
{
public:
    String Name;
    double X = 0, Y = 0;
    double Direction = 90;
    double Size = 100;
    bool Visible = true;
    RotationStyle Rotation = RotationStyle::AllAround;
    bool IsClone = false;
    StageBase* BaseStage = nullptr;

    // Pen state (Scratch defaults).
    bool PenIsDown = false;
    double PenColor = 66.66, PenSaturation = 100, PenBrightness = 100, PenTransparency = 0;
    double PenSize = 1;
    float PenR = 0, PenG = 0, PenB = 1, PenA = 1;

    SpriteBase() { UpdatePenColor(); }
    bool IsStage() const override { return false; }

    // ---------- reporters ----------
    double XPosition() const { return LimitPrecision(X); }
    double YPosition() const { return LimitPrecision(Y); }
    double SizeReporter() const { return ops::Round(Size); }

    // ---------- motion ----------
    void GoTo(double x, double y);
    void SetX(double x) { GoTo(x, Y); }
    void SetY(double y) { GoTo(X, y); }
    void ChangeX(double dx) { GoTo(X + dx, Y); }
    void ChangeY(double dy) { GoTo(X, Y + dy); }
    void PointInDirection(double d);
    void TurnRight(double deg) { PointInDirection(Direction + deg); }
    void TurnLeft(double deg) { PointInDirection(Direction - deg); }

    // ---------- looks ----------
    void SetSize(double s);
    void ChangeSize(double s) { SetSize(Size + s); }
    void Show();
    void Hide();

    // ---------- pen ----------
    void PenDown();
    void PenUp() { PenIsDown = false; }
    void Stamp();
    void SetPenSize(double s) { PenSize = ops::JsMax(0.0, s); } // high-quality pen: no 1..1200 limit
    void ChangePenSize(double s) { SetPenSize(PenSize + s); }
    void SetPenColor(const Value& color);
    void SetPenParam(PenParam p, double v) { SetOrChangePenParam(p, v, false); }
    void ChangePenParam(PenParam p, double v) { SetOrChangePenParam(p, v, true); }

    // ---------- sensing ----------
    bool TouchingMouse() const;

    // ---------- clones ----------
    void CreateClone();
    void DeleteThisClone();

protected:
    void OnVisualChange() override;
    /// A copy of this sprite for "create clone" (generated per sprite class).
    virtual SpriteBase* CloneSelf() const = 0;

private:
    static double LimitPrecision(double c)
    {
        double r = ops::Round(c);
        return std::fabs(c - r) < 1e-9 ? r : c;
    }
    void SetOrChangePenParam(PenParam p, double v, bool change);
    void UpdatePenColor();
};

/// Scratch's color conversions (scratch-vm util/color.js and cast.js).
namespace ColorUtil {
struct Rgba { int r, g, b, a; bool hasAlpha; };
Rgba ToRgb(const Value& v);
void RgbToHsv(int r, int g, int b, double& h, double& s, double& v);
void HsvToRgb(double h, double s, double v, int& r, int& g, int& b);
} // namespace ColorUtil

} // namespace scratch
