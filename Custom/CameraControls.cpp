// Hand-written additions to the generated Glue3DSprite (not overwritten by the converter):
// Unity-style camera controls for the 3D scene view. Settings are at the top of Glue3DSprite.members.h.
#include <algorithm>

#include "Engine.h"
#include "Glue3DSprite.g.h"

namespace Glue3D {

// ---------------- right-mouse look ----------------

double Glue3DSprite::MouseLookX() { return RightMouseDown() ? LookDeltaX() * MouseLookSensitivity : 0; }

double Glue3DSprite::MouseLookY()
{
    return RightMouseDown() ? LookDeltaY() * MouseLookSensitivity * (InvertMouseLookY ? -1 : 1) : 0;
}

// ---------------- wheel zoom, middle-button pan, fly up/down ----------------

/// Called at the end of the camera block every frame. Adds what Unity's scene view does:
/// mouse wheel moves forward/back along the view, middle-drag slides the camera,
/// and while the right button is held W/S also follow the up/down angle and Q/E move down/up.
void Glue3DSprite::UnityStyleCameraExtras(Value speed)
{
    double yaw = stage.ramEngineData.Item(118).Num();
    double pitch = stage.ramEngineData.Item(120).Num();
    double camX = stage.ramEngineData.Item(112).Num();
    double camY = stage.ramEngineData.Item(114).Num();
    double camZ = stage.ramEngineData.Item(116).Num();
    double s = speed.Num();

    // Forward along the view direction, including looking up/down.
    double fx = Sin(yaw) * Cos(pitch), fy = Sin(pitch), fz = Cos(yaw) * Cos(pitch);
    // Right of the view (same direction the "d" key moves).
    double rx = Sin(90 + yaw), rz = Cos(90 + yaw);

    auto& input = GetEngine().Input;
    double wheel = input.WheelMove;
    if (wheel != 0 && SceneViewActive() && InSceneViewport(MouseX(), MouseY()))
    {
        camX += fx * wheel * WheelZoomStep;
        camY += fy * wheel * WheelZoomStep;
        camZ += fz * wheel * WheelZoomStep;
    }

    if (input.MiddleMouseDown)
    {
        // Drag the scene with the mouse: the camera moves the opposite way.
        camX -= rx * input.PanDeltaX * PanSpeed;
        camZ -= rz * input.PanDeltaX * PanSpeed;
        camY -= input.PanDeltaY * PanSpeed;
    }

    if (RightMouseDown())
    {
        // W/S already move horizontally; add the up/down part of the view direction.
        camY += fy * (Num(KeyPressed(u"w"_s)) - Num(KeyPressed(u"s"_s))) * s;
        camY += (Num(KeyPressed(u"e"_s)) - Num(KeyPressed(u"q"_s))) * s;
    }

    if (camY < -0.25) camY = -0.25; // same floor limit as the original camera
    CreateAddress(u"engine.camera_x"_s, camX, 56);
    CreateAddress(u"engine.camera_y"_s, camY, 57);
    CreateAddress(u"engine.camera_z"_s, camZ, 58);
}

// ---------------- game controls (game.movement's 3rd and 4th values) ----------------
//
//   game.movement(val 0.1, val 1, str arrows, str left/right)    the original: Left/Right turn
//   game.movement(val 0.1, val 1, str mouse, str both)           mouse look all around (like most 3D games)
//   game.movement(val 0.1, val 1, str both, str both)            arrows and mouse both look around
//
//   controls  "arrows" (default) | "mouse" | "both"         what turns the view
//   view      "left/right" (default) | "up/down" | "both"   which way the view turns
//
// Arrow keys that don't turn the view move the player: Up/Down walk forward/back, Left/Right step sideways.
// W/A/S/D always move. For "vehicle" movement the view value only adds looking up/down; steering is
// A/D or Left/Right (and the mouse with mouse controls), driving is W/S (or Up/Down when they don't look).

static bool Held(Glue3DSprite& g, const String& key) { return g.KeyPressed(key); }

/// "Up\Down", "up-down", "UP/DOWN"... -> "updown"
static std::string Letters(const Value& v)
{
    std::string out;
    for (char c : v.Str().Lower().ToUtf8())
        if (c >= 'a' && c <= 'z') out.push_back(c);
    return out;
}

void Glue3DSprite::SetGameControls(const Value& controls, const Value& view)
{
    std::string c = Letters(controls), v = Letters(view);
    GameControls = (c == "mouse" || c == "both") ? c : "arrows"; // anything else (or nothing): arrow keys
    if (v == "updown" || v == "vertical" || v == "ud") GameView = "updown";
    else if (v == "both" || v == "all") GameView = "both";
    else GameView = "leftright"; // the original behavior
    gameMovementFrames_ = 2;
    if (GameUsesMouse()) gameMouseFrames_ = 2; // keeps the mouse locked while the game runs
}

bool Glue3DSprite::GameUsesMouse() { return GameControls == "mouse" || GameControls == "both"; }
bool Glue3DSprite::GameUsesArrows() { return GameControls == "arrows" || GameControls == "both"; }
bool Glue3DSprite::GameViewLeftRight() { return GameView == "leftright" || GameView == "both"; }
bool Glue3DSprite::GameViewUpDown() { return GameView == "updown" || GameView == "both"; }

/// +1 forward, -1 back: W/S, and Up/Down unless they look up/down.
double Glue3DSprite::MoveForwardAxis()
{
    bool arrowsMove = !(GameUsesArrows() && GameViewUpDown());
    bool fwd = Held(*this, u"w"_s) || (arrowsMove && Held(*this, u"up arrow"_s));
    bool back = Held(*this, u"s"_s) || (arrowsMove && Held(*this, u"down arrow"_s));
    return Num(fwd) - Num(back);
}

/// +1 right, -1 left (the original D/A direction): A/D, and Left/Right unless they turn the view.
double Glue3DSprite::MoveStrafeAxis()
{
    bool arrowsMove = !(GameUsesArrows() && GameViewLeftRight());
    bool right = Held(*this, u"d"_s) || (arrowsMove && Held(*this, u"right arrow"_s));
    bool left = Held(*this, u"a"_s) || (arrowsMove && Held(*this, u"left arrow"_s));
    return Num(right) - Num(left);
}

/// Arrow-key turning left/right: +1 right, -1 left.
double Glue3DSprite::TurnAxis()
{
    if (!(GameUsesArrows() && GameViewLeftRight())) return 0;
    return Num(Held(*this, u"right arrow"_s)) - Num(Held(*this, u"left arrow"_s));
}

/// Degrees to add to the player's direction from mouse movement this frame.
double Glue3DSprite::MouseTurn(Value turnSpeed)
{
    if (!GameUsesMouse() || !GameViewLeftRight()) return 0;
    auto& input = GetEngine().Input;
    bool active = input.MouseCaptured() || input.RightMouseDown; // (right-drag also works, e.g. before locking)
    if (!active) return 0;
    double speed = turnSpeed.Num() == 0 ? 1 : turnSpeed.Num();
    return -input.LookDeltaX * GameMouseSensitivity * speed; // moving right turns right, like the right arrow
}

/// Vehicle steering: +1 right, -1 left (keys, plus the mouse with mouse/both).
double Glue3DSprite::SteerAxis()
{
    double steer = Num(Held(*this, u"d"_s) || Held(*this, u"right arrow"_s)) - Num(Held(*this, u"a"_s) || Held(*this, u"left arrow"_s));
    if (GameUsesMouse())
    {
        auto& input = GetEngine().Input;
        if (input.MouseCaptured() || input.RightMouseDown) steer += input.LookDeltaX / 4;
    }
    return std::clamp(steer, -1.0, 1.0);
}

void Glue3DSprite::GameLook(Value turnSpeed)
{
    if (!GameViewUpDown()) return;
    double speed = turnSpeed.Num() == 0 ? 1 : turnSpeed.Num();
    if (GameUsesArrows())
    {
        // the same rate as turning with Left/Right
        double rate = Nz((speed * (60 * stage.timeDelta.Num())) / stage.timeClockSpeed.Num());
        gamePitch_ += (Num(Held(*this, u"up arrow"_s)) - Num(Held(*this, u"down arrow"_s))) * rate;
    }
    auto& input = GetEngine().Input;
    if (GameUsesMouse() && (input.MouseCaptured() || input.RightMouseDown)) gamePitch_ += input.LookDeltaY * GameMouseSensitivity * speed;
    gamePitch_ = std::clamp(gamePitch_, -GameMaxPitch, GameMaxPitch);

    // The game camera looks up/down: Glue3D turns a camera object's "ry" into the view's pitch (ry * -1.5).
    Value cam = stage.engineGamecamera;
    if (Eq(cam, u""_s) || stage.scene.IndexOf(cam.Str() + u".start"_s) == 0) return;
    EditDataInObjectDataOperation(cam, u"ry"_s, -gamePitch_ / 1.5, u"set"_s);
}

/// Is a stage point inside the 3D game view?
bool Glue3DSprite::InGameView(double x, double y)
{
    return x > stage.engineScreenTopleftx.Num() && x < stage.engineScreenBottomrightx.Num() &&
           y < stage.engineScreenToplefty.Num() && y > stage.engineScreenBottomrighty.Num();
}

/// Before scripts run: lock the mouse when the game view is clicked (mouse controls), free it when the game stops.
void Glue3DSprite::GameControlsUpdate()
{
    ScriptBlocksUpdate(); // key presses for game.key_down / key_up (Custom/ScriptSystems.cpp)
    auto& input = GetEngine().Input;
    bool down = input.MouseDown;
    bool pressed = down && !gamePrevMouseDown_;
    gamePrevMouseDown_ = down;
    if (gameMouseFrames_ > 0) gameMouseFrames_--;
    if (gameMovementFrames_ > 0 && --gameMovementFrames_ == 0) gamePitch_ = 0; // the game stopped: level view next time
    bool wanted = gameMouseFrames_ > 0;
    if (!wanted)
    {
        if (input.MouseCaptured()) input.SetMouseCapture(false);
        return;
    }
    if (!input.MouseCaptured() && pressed && InGameView(MouseX(), MouseY())) input.SetMouseCapture(true);
}

/// A hint while mouse controls are waiting for a click.
void Glue3DSprite::GameControlsDraw(Rect stageRect)
{
    ScriptBlocksDraw(stageRect); // game.message (Custom/ScriptBlocks.cpp)
    auto& input = GetEngine().Input;
    if (gameMouseFrames_ <= 0 || input.MouseCaptured()) return;
    std::string text = "click the game to use the mouse  -  Esc frees it";
    float k = stageRect.w / 480.0f, size = 10 * k;
    auto& mon = GetEngine().monitors;
    float w = mon.Measure(text, size);
    float x = StageToWindowX(stageRect, (stage.engineScreenTopleftx.Num() + stage.engineScreenBottomrightx.Num()) / 2) - w / 2;
    float y = StageToWindowY(stageRect, stage.engineScreenBottomrighty.Num()) - size - 8 * k;
    gfx::DrawRectRounded({x - 5 * k, y - 2 * k, w + 10 * k, size + 4 * k}, 0.4f, 4, {20, 20, 20, 170});
    mon.Text(text, x, y, size, Color::White());
}

} // namespace Glue3D
