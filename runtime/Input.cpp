#include "Input.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <deque>
#include <unordered_map>

#include "Engine.h"
#include "Ops.h"

namespace scratch {

namespace {

// Events from GLFW callbacks, collected between frames.
struct Pending
{
    std::vector<std::pair<int, int>> keys; // (key, action)
    std::deque<uint32_t> chars;
    double scroll = 0, scrollX = 0;
    bool buttonPressed[3] = {false, false, false};
};
Pending pending;

void OnKey(GLFWwindow*, int key, int, int action, int) { pending.keys.push_back({key, action}); }
void OnChar(GLFWwindow*, unsigned int cp) { pending.chars.push_back(cp); }
void OnScroll(GLFWwindow*, double x, double y)
{
    pending.scroll += y;
    pending.scrollX -= x; // + = towards the start (left), like the vertical wheel's up
}
void OnButton(GLFWwindow*, int button, int action, int)
{
    if (action == GLFW_PRESS && button >= 0 && button < 3) pending.buttonPressed[button] = true;
}

const std::unordered_map<int, const char16_t*>& Named()
{
    static const std::unordered_map<int, const char16_t*> m = {
        {GLFW_KEY_SPACE, u"space"},         {GLFW_KEY_LEFT, u"left arrow"},   {GLFW_KEY_RIGHT, u"right arrow"},
        {GLFW_KEY_UP, u"up arrow"},         {GLFW_KEY_DOWN, u"down arrow"},   {GLFW_KEY_ENTER, u"enter"},
        {GLFW_KEY_KP_ENTER, u"enter"},      {GLFW_KEY_BACKSPACE, u"backspace"}, {GLFW_KEY_DELETE, u"delete"},
        {GLFW_KEY_LEFT_SHIFT, u"shift"},    {GLFW_KEY_RIGHT_SHIFT, u"shift"}, {GLFW_KEY_CAPS_LOCK, u"caps lock"},
        {GLFW_KEY_SCROLL_LOCK, u"scroll lock"}, {GLFW_KEY_LEFT_CONTROL, u"control"}, {GLFW_KEY_RIGHT_CONTROL, u"control"},
        {GLFW_KEY_ESCAPE, u"escape"},       {GLFW_KEY_INSERT, u"insert"},     {GLFW_KEY_HOME, u"home"},
        {GLFW_KEY_END, u"end"},             {GLFW_KEY_PAGE_UP, u"page up"},   {GLFW_KEY_PAGE_DOWN, u"page down"},
    };
    return m;
}

bool IsKeyName(const String& s)
{
    if (s == u"any") return true;
    for (auto& [k, v] : Named())
        if (s == std::u16string_view(v)) return true;
    return false;
}

/// The character a key types on a US keyboard (used when the OS doesn't report one).
int UsLayoutChar(int k)
{
    if (k >= 'A' && k <= 'Z') return k;
    if (k >= '0' && k <= '9') return k;
    if (k >= GLFW_KEY_KP_0 && k <= GLFW_KEY_KP_9) return '0' + (k - GLFW_KEY_KP_0);
    switch (k)
    {
    case GLFW_KEY_GRAVE_ACCENT: return '`';
    case GLFW_KEY_MINUS: return '-';
    case GLFW_KEY_EQUAL: return '=';
    case GLFW_KEY_LEFT_BRACKET: return '[';
    case GLFW_KEY_RIGHT_BRACKET: return ']';
    case GLFW_KEY_BACKSLASH: return '\\';
    case GLFW_KEY_SEMICOLON: return ';';
    case GLFW_KEY_APOSTROPHE: return '\'';
    case GLFW_KEY_COMMA: return ',';
    case GLFW_KEY_PERIOD: return '.';
    case GLFW_KEY_SLASH: return '/';
    case GLFW_KEY_KP_DECIMAL: return '.';
    case GLFW_KEY_KP_DIVIDE: return '/';
    case GLFW_KEY_KP_MULTIPLY: return '*';
    case GLFW_KEY_KP_SUBTRACT: return '-';
    case GLFW_KEY_KP_ADD: return '+';
    default: return 0;
    }
}

} // namespace

void InputState::Attach(void* w)
{
    window_ = w;
    auto* win = static_cast<GLFWwindow*>(w);
    glfwSetKeyCallback(win, OnKey);
    glfwSetCharCallback(win, OnChar);
    glfwSetScrollCallback(win, OnScroll);
    glfwSetMouseButtonCallback(win, OnButton);
    glfwGetCursorPos(win, &lastCursorX_, &lastCursorY_);
}

bool InputState::IsPhysicalKeyDown(int key) const
{
    return window_ && glfwGetKey(static_cast<GLFWwindow*>(window_), key) == GLFW_PRESS;
}

void InputState::Update(Engine& engine, Rect stage)
{
    auto* win = static_cast<GLFWwindow*>(window_);
    Pending ev = std::move(pending);
    pending = Pending{};

    // ---- mouse ----
    double cx = 0, cy = 0;
    glfwGetCursorPos(win, &cx, &cy);
    double rawDx = cx - lastCursorX_, rawDy = cy - lastCursorY_;
    lastCursorX_ = cx;
    lastCursorY_ = cy;
    int winW, winH, fbW, fbH;
    glfwGetWindowSize(win, &winW, &winH);
    glfwGetFramebufferSize(win, &fbW, &fbH);
    double dpi = winW > 0 ? (double)fbW / winW : 1;
    bool leftDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS || ev.buttonPressed[0];
    bool rightDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS || ev.buttonPressed[1];
    bool middleDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS || ev.buttonPressed[2];
    bool leftPressed = ev.buttonPressed[0] || (leftDown && !prevLeft_);
    bool rightPressed = ev.buttonPressed[1] || (rightDown && !prevRight_);
    prevLeft_ = leftDown;
    prevRight_ = rightDown;

    if (MouseOverride)
    {
        MouseX = MouseOverride->x;
        MouseY = MouseOverride->y;
        MouseDown = MouseOverride->down;
        LookOverrideT look = LookOverride.value_or(LookOverrideT{false, 0, 0});
        RightMouseDown = look.down;
        LookDeltaX = look.dx;
        LookDeltaY = look.dy;
    }
    else if (captured_)
    {
        // Game mouse look: the pointer stays put; movement turns the view.
        bool escape = false;
        for (auto [k, action] : ev.keys) if (k == GLFW_KEY_ESCAPE && action == GLFW_PRESS) escape = true;
        if (escape || !glfwGetWindowAttrib(win, GLFW_FOCUSED)) SetMouseCapture(false);
        LookDeltaX = rawDx * dpi * 480 / stage.w;
        LookDeltaY = -rawDy * dpi * 360 / stage.h;
        RightMouseDown = false;
        WheelMove = ev.scroll;
        WheelMoveX = ev.scrollX;
        MiddleMouseDown = false;
        PanDeltaX = PanDeltaY = 0;
        MouseDown = leftDown;
    }
    else
    {
        double px = cx * dpi, py = cy * dpi;
        bool inside = px >= stage.x && py >= stage.y && px < stage.x + stage.w && py < stage.y + stage.h;
        UpdateMouseLook(dpi, stage, inside, rawDx, rawDy, rightPressed);
        if (!looking_) inside = px >= stage.x && py >= stage.y && px < stage.x + stage.w && py < stage.y + stage.h;
        WheelMove = inside ? ev.scroll : 0;
        WheelMoveX = inside ? ev.scrollX : 0;
        MiddleMouseDown = middleDown && inside;
        PanDeltaX = MiddleMouseDown ? rawDx * dpi * 480 / stage.w : 0;
        PanDeltaY = MiddleMouseDown ? -rawDy * dpi * 360 / stage.h : 0;
        if (!looking_) // the pointer stays put while looking around
        {
            MouseX = ops::Clamp(480 * ((px - stage.x) / stage.w - 0.5), -240, 240);
            MouseY = ops::Clamp(-360 * ((py - stage.y) / stage.h - 0.5), -180, 180);
        }
        if (leftPressed) pressStartedOnStage_ = inside;
        MouseDown = leftDown && pressStartedOnStage_;
    }

    // Shift + wheel scrolls sideways (and then not up/down).
    if (ShiftHeld() && WheelMove != 0)
    {
        WheelMoveX += WheelMove;
        WheelMove = 0;
    }
    // Scratch turns mouse-wheel scrolling into up/down arrow "key pressed" events (Glue3D scrolls lists this way).
    if (WheelMove > 0) engine.KeyPressed(u"up arrow"_s);
    else if (WheelMove < 0) engine.KeyPressed(u"down arrow"_s);

    // ---- keyboard: pair each pressed key with the character it typed (layout and Shift aware) ----
    pressedThisFrame_.clear();
    std::deque<uint32_t>& chars = ev.chars;
    if (TextCapture)
    {
        // typing into a text box: nothing is "pressed" for the game
        for (uint32_t c : chars)
        {
            if (c < 0x80) Typed += (char)c;
            else if (c < 0x800) { Typed += (char)(0xC0 | (c >> 6)); Typed += (char)(0x80 | (c & 0x3F)); }
            else if (c < 0x10000) { Typed += (char)(0xE0 | (c >> 12)); Typed += (char)(0x80 | ((c >> 6) & 0x3F)); Typed += (char)(0x80 | (c & 0x3F)); }
            else { Typed += (char)(0xF0 | (c >> 18)); Typed += (char)(0x80 | ((c >> 12) & 0x3F)); Typed += (char)(0x80 | ((c >> 6) & 0x3F)); Typed += (char)(0x80 | (c & 0x3F)); }
        }
        for (auto [k, action] : ev.keys)
        {
            if (action == GLFW_PRESS) pressedThisFrame_.insert(k);
            if (action == GLFW_PRESS || action == GLFW_REPEAT) TypedKeys.push_back(k);
        }
        pressed_.clear();
        physical_.clear();
        return;
    }
    for (auto [k, action] : ev.keys)
    {
        if (action == GLFW_PRESS) pressedThisFrame_.insert(k);
        if (action == GLFW_REPEAT)
        {
            // Held keys repeat "when key pressed" like the browser's key repeat.
            auto it = physical_.find(k);
            if (it != physical_.end()) engine.KeyPressed(it->second);
            continue;
        }
        if (action != GLFW_PRESS) continue;
        String key;
        auto named = Named().find(k);
        if (named != Named().end()) key = String(std::u16string_view(named->second));
        else
        {
            int us = UsLayoutChar(k);
            if (!us) continue;
            key = String::Char((char16_t)us);
            if (!chars.empty())
            {
                uint32_t c = chars.front();
                chars.pop_front();
                if (c < 0x10000) key = String::Char((char16_t)c).Upper();
            }
        }
        physical_[k] = key;
        KeyDown(engine, key);
    }
    for (auto it = physical_.begin(); it != physical_.end();)
    {
        if (glfwGetKey(win, it->first) == GLFW_RELEASE)
        {
            KeyUp(it->second);
            it = physical_.erase(it);
        }
        else ++it;
    }
}

void InputState::UpdateMouseLook(double dpi, Rect stage, bool inside, double dx, double dy, bool rightPressed)
{
    auto* win = static_cast<GLFWwindow*>(window_);
    LookDeltaX = LookDeltaY = 0;
    if (!looking_ && inside && rightPressed)
    {
        looking_ = true;
        lookJustStarted_ = true;
        glfwGetCursorPos(win, &lookStartX_, &lookStartY_);
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported()) glfwSetInputMode(win, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        RightMouseDown = true;
        return;
    }
    if (looking_ && glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS)
    {
        looking_ = false;
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSetCursorPos(win, lookStartX_, lookStartY_);
        lastCursorX_ = lookStartX_;
        lastCursorY_ = lookStartY_;
    }
    RightMouseDown = looking_;
    if (!looking_) return;
    if (lookJustStarted_) { lookJustStarted_ = false; return; } // first frame: the cursor was just captured
    LookDeltaX = dx * dpi * 480 / stage.w;
    LookDeltaY = -dy * dpi * 360 / stage.h;
}

bool InputState::ShiftHeld() const
{
    return IsPhysicalKeyDown(GLFW_KEY_LEFT_SHIFT) || IsPhysicalKeyDown(GLFW_KEY_RIGHT_SHIFT) ||
           std::find(pressed_.begin(), pressed_.end(), u"shift"_s) != pressed_.end();
}

void InputState::SetMouseCapture(bool on)
{
    if (on == captured_ || !window_) return;
    auto* win = static_cast<GLFWwindow*>(window_);
    captured_ = on;
    if (on)
    {
        looking_ = false; // a right-drag look in progress ends
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported()) glfwSetInputMode(win, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
    else glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwGetCursorPos(win, &lastCursorX_, &lastCursorY_); // no jump on the next frame
}

void InputState::KeyDown(Engine& engine, const String& key)
{
    if (std::find(pressed_.begin(), pressed_.end(), key) == pressed_.end()) pressed_.push_back(key);
    engine.KeyPressed(key);
}

void InputState::KeyUp(const String& key)
{
    auto it = std::find(pressed_.begin(), pressed_.end(), key);
    if (it != pressed_.end()) pressed_.erase(it);
}

bool InputState::IsKeyDown(const Value& arg) const
{
    String key;
    if (arg.IsNumber())
    {
        double n = arg.RawNumber();
        if (n >= 48 && n <= 90) key = String::Char((char16_t)(int)n);
        else if (n == 32) key = u"space"_s;
        else if (n == 37) key = u"left arrow"_s;
        else if (n == 38) key = u"up arrow"_s;
        else if (n == 39) key = u"right arrow"_s;
        else if (n == 40) key = u"down arrow"_s;
        else key = arg.Str();
    }
    else key = arg.Str();
    if (key == u"any") return !pressed_.empty();
    if (!(key.size() > 1 && IsKeyName(key)))
    {
        if (key.size() > 1) key = key.Substr(0, 1);
        if (key == u" ") key = u"space"_s;
        else if (key == u"\r") key = u"enter"_s;
        else if (key == u"\x1b") key = u"escape"_s;
        else key = key.Upper();
    }
    return std::find(pressed_.begin(), pressed_.end(), key) != pressed_.end();
}

} // namespace scratch
