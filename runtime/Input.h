// Mouse and keyboard (through GLFW), mapped to Scratch's stage coordinates and key names.
#pragma once
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "Gfx.h"
#include "Value.h"

namespace scratch {

class Engine;

/// Physical key codes (the same numbers GLFW uses), for editor shortcuts.
namespace Key {
constexpr int A = 65, C = 67, D = 68, E = 69, Q = 81, S = 83, V = 86, W = 87, Z = 90;
constexpr int Escape = 256, Enter = 257, Tab = 258, Backspace = 259, Delete = 261;
constexpr int Right = 262, Left = 263, Down = 264, Up = 265, F11 = 300;
constexpr int LeftShift = 340, LeftControl = 341, LeftAlt = 342, RightShift = 344, RightControl = 345, RightAlt = 346;
} // namespace Key

class InputState
{
public:
    double MouseX = 0, MouseY = 0;
    bool MouseDown = false;

    /// Set by the automation driver (testing) to replace the real mouse.
    struct MouseOverrideT { double x, y; bool down; };
    struct LookOverrideT { bool down; double dx, dy; };
    std::optional<MouseOverrideT> MouseOverride;
    std::optional<LookOverrideT> LookOverride;

    /// Right mouse button held for "mouse look" (not a Scratch feature; used by the 3D camera).
    bool RightMouseDown = false;
    /// How far the mouse moved this frame while the right button is held, in stage units (up = positive).
    double LookDeltaX = 0, LookDeltaY = 0;
    /// Mouse wheel this frame (positive = away from you) and middle-button drag in stage units.
    double WheelMove = 0, PanDeltaX = 0, PanDeltaY = 0;
    /// Sideways scrolling this frame: a touchpad / tilt wheel, or the normal wheel while Shift is held.
    double WheelMoveX = 0;
    /// Is Shift held (a real key, or one pressed by the automation driver)?
    bool ShiftHeld() const;
    bool MiddleMouseDown = false;

    /// Connects to the window's input events.
    void Attach(void* glfwWindow);
    void Update(Engine& engine, Rect stage);

    void KeyDown(Engine& engine, const String& key);
    void KeyUp(const String& key);
    /// Scratch's "key (...) pressed?" argument rules.
    bool IsKeyDown(const Value& arg) const;

    /// Mouse capture for games ("mouse look"): the cursor is hidden and locked to the window, and every mouse
    /// movement is reported in LookDeltaX/Y. Escape (or leaving the window) releases it.
    void SetMouseCapture(bool on);
    bool MouseCaptured() const { return captured_; }

    /// While true (a text box such as the game's console is open), keys type text instead of being pressed:
    /// characters go to Typed (UTF-8) and editing keys (GLFW codes: Enter, Backspace, Escape…) to TypedKeys.
    bool TextCapture = false;
    std::string Typed;
    std::vector<int> TypedKeys;

    /// Physical keys (Key:: codes) for editor shortcuts.
    bool IsPhysicalKeyDown(int key) const;
    bool IsPhysicalKeyPressed(int key) const { return pressedThisFrame_.count(key) > 0; }

private:
    void* window_ = nullptr;
    std::vector<String> pressed_;          // keys held, as Scratch names them ("A", "space", ...)
    std::map<int, String> physical_;       // physical key -> Scratch name it produced
    std::set<int> pressedThisFrame_;
    bool pressStartedOnStage_ = false;
    bool looking_ = false, lookJustStarted_ = false;
    double lookStartX_ = 0, lookStartY_ = 0;
    double lastCursorX_ = 0, lastCursorY_ = 0;
    bool prevLeft_ = false, prevRight_ = false;
    bool captured_ = false;

    void UpdateMouseLook(double dpi, Rect stage, bool inside, double dx, double dy, bool rightPressed);
};

} // namespace scratch
