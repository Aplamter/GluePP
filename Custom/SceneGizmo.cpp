// Hand-written additions to the generated Glue3DSprite (not overwritten by the converter):
// a Unity-style toolbar (Select / Move / Rotate / Scale + gizmo on/off) and 3D gizmos for the scene view.
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Engine.h"
#include "Glue3DSprite.g.h"

namespace Glue3D {

namespace {

/// Gizmo arm length on screen, in stage units.
constexpr double GizmoScreenSize = 42;
/// Snapping while Ctrl is held.
constexpr double MoveSnap = 0.25, RotateSnap = 15, ScaleSnap = 0.1;
constexpr int CenterHandle = 3, GizmoButton = 4;
constexpr double ButtonSize = 17, ButtonGap = 2;

const char* const AxisNames[] = {"x", "y", "z"};
const Color AxisColors[] = {{232, 64, 64, 255}, {110, 204, 60, 255}, {66, 128, 244, 255}};
constexpr Color HotColor{255, 214, 0, 255};
const char* const ButtonTips[] = {"Select (opens Glue3D's object menu)", "Move", "Rotate", "Scale",
                                  "Turn the gizmo off (clicks open Glue3D's object menu)"};
const char* const GizmoOffTip = "Turn the gizmo on (move / rotate / scale)";

const char* const MoveKeys[] = {"x", "y", "z"};
const char* const RotateKeys[] = {"ry", "rx", "rz"}; // rings around the X, Y and Z axes (rx is yaw in Glue3D)
const char* const ScaleKeys[] = {"sx", "sy", "sz"};

Value T(const std::string& s) { return Value(String::FromUtf8(s)); }

double DistToSegment(Vec2 p, Vec2 a, Vec2 b)
{
    Vec2 ab = b - a;
    float t = ab.LengthSquared() < 1e-6f ? 0 : std::clamp(Vec2::Dot(p - a, ab) / ab.LengthSquared(), 0.0f, 1.0f);
    return Vec2::Distance(p, a + ab * t);
}

Vec3 Axis(int i) { return i == 0 ? Vec3(1, 0, 0) : i == 1 ? Vec3(0, 1, 0) : Vec3(0, 0, 1); }

/// Point on a rotation ring: ring i goes around axis i.
Vec3 RingPoint(int i, double angle)
{
    float c = (float)std::cos(angle), s = (float)std::sin(angle);
    if (i == 0) return {0, c, s};
    if (i == 1) return {s, 0, c};
    return {c, s, 0};
}

double WrapAngle(double a)
{
    a = std::fmod(a, 360);
    if (a > 180) a -= 360;
    if (a <= -180) a += 360;
    return a;
}

double Snap(double v, double step, bool on) { return on ? std::round(v / step) * step : v; }

std::string Fmt(double v, const char* f)
{
    char buf[64];
    std::snprintf(buf, sizeof buf, f, v);
    return buf;
}

/// Like C#'s "0.##": up to 2 decimals, no trailing zeros.
std::string Short(double v)
{
    std::string s = Fmt(v, "%.2f");
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    if (s == "-0") s = "0";
    return s;
}

} // namespace

// ---------------- where the 3D view is ----------------

bool Glue3DSprite::SceneViewActive()
{
    return !MainMenuActive && Eq(stage.ramEngineData.Item(76), u"configured"_s) && Eq(stage.editorTab, u"scene"_s) &&
           Eq(stage.allRun, 0) && Eq(stage.engineFullscreen, 0) && Eq(stage.guiFreeze, 0) &&
           Eq(stage.ramEngineData.Item(26), 0) &&   // no dropdown/menu open
           Eq(stage.editorSceneTool, u""_s);        // none of Glue3D's own scene tools in use
}

double Glue3DSprite::ViewLeft() { return stage.engineScreenTopleftx.Num(); }
double Glue3DSprite::ViewTop() { return stage.engineScreenToplefty.Num(); }
double Glue3DSprite::ViewRight() { return stage.engineScreenBottomrightx.Num(); }
double Glue3DSprite::ViewBottom() { return stage.engineScreenBottomrighty.Num(); }

bool Glue3DSprite::InSceneViewport(double x, double y) { return x > ViewLeft() && x < ViewRight() && y < ViewTop() && y > ViewBottom(); }

// ---------------- 3D <-> screen, the same math as the engine's "project point" block ----------------

Glue3DSprite::CameraInfo Glue3DSprite::Camera()
{
    double yaw = stage.ramEngineData.Item(118).Num();
    double pm = std::fabs(stage.enginePerspectivemultiplier.Num());
    bool stretched = Eq(stage.ramEngineData.Item(178), 0);
    return {stage.ramEngineData.Item(112).Num(), stage.ramEngineData.Item(114).Num(), stage.ramEngineData.Item(116).Num(),
            Sin(yaw), Cos(yaw), stage.engineFov.Num(), stage.ramEngineData.Item(120).Num(),
            stretched ? 1.15 : 1, stretched ? 1.15 + 0.06 * pm : 1};
}

/// World point to stage coordinates. False if it is behind the camera.
bool Glue3DSprite::Project(double x, double y, double z, Vec2& screen, double& depth)
{
    CameraInfo c = Camera();
    double ox = x - c.camX, oy = y - c.camY, oz = z - c.camZ;
    double nx = c.cos * ox - c.sin * oz, nz = c.sin * ox + c.cos * oz;
    depth = nz;
    screen = {};
    if (nz <= 0.05) return false;
    double w = ViewRight() - ViewLeft(), h = ViewTop() - ViewBottom();
    double midX = stage.engineScreenMidx.Num(), midY = stage.engineScreenMidy.Num();
    double sx = c.kx * (midX + w * (nx * c.fov / nz) / 480);
    double sy = c.ky * (midY + h * (oy * c.fov / nz - 3 * c.pitch) / 360);
    screen = {(float)sx, (float)sy};
    return true;
}

bool Glue3DSprite::ProjectV(Vec3 p, Vec2& s)
{
    double depth;
    return Project(p.x, p.y, p.z, s, depth);
}

/// The point on the horizontal plane at height y that is under a stage-coordinate position.
bool Glue3DSprite::UnprojectToPlane(double sx, double sy, double planeY, double& x, double& z)
{
    CameraInfo c = Camera();
    x = z = 0;
    double w = ViewRight() - ViewLeft(), h = ViewTop() - ViewBottom();
    double u = (sx / c.kx - stage.engineScreenMidx.Num()) * 480 / w;             // = nx * fov / nz
    double v = (sy / c.ky - stage.engineScreenMidy.Num()) * 360 / h + 3 * c.pitch; // = ny * fov / nz
    double ny = planeY - c.camY;
    if (std::fabs(v) < 1e-6) return false;
    double nz = ny * c.fov / v;
    if (nz <= 0.05 || nz > 500) return false;
    double nx = u * nz / c.fov;
    x = c.cos * nx + c.sin * nz + c.camX;
    z = -c.sin * nx + c.cos * nz + c.camZ;
    return true;
}

/// World length that appears about GizmoScreenSize stage units long at this depth.
double Glue3DSprite::ArmLength(double depth)
{
    return GizmoScreenSize * depth * 480 / ((ViewRight() - ViewLeft()) * std::max(1.0, Camera().fov));
}

// ---------------- scene objects ----------------

/// Objects in the scene that can be moved in 3D (UI elements are 2D, so they're skipped).
std::vector<Glue3DSprite::SceneObject> Glue3DSprite::SceneObjects()
{
    std::vector<SceneObject> list;
    int index = 2;
    int count = (int)stage.scene.Item(1).Num();
    for (int i = 0; i < count && index <= stage.scene.Length(); i++)
    {
        std::string type = stage.scene.Item(index + 5).Str().ToUtf8();
        std::string model = stage.scene.Item(index + 7).Str().ToUtf8();
        if (model != "elementui")
            list.push_back({stage.scene.Item(index + 3).Str().ToUtf8(), type, stage.scene.Item(index + 11).Num(),
                            stage.scene.Item(index + 13).Num(), stage.scene.Item(index + 15).Num()});
        int size = (int)stage.scene.Item(index).Num();
        if (size <= 0) break;
        index += size;
    }
    return list;
}

double Glue3DSprite::ReadObject(const std::string& name, const char* key)
{
    EditDataInObjectDataOperation(T(name), T(key), 0, u"read"_s);
    return stage.dataReturn.Num();
}

void Glue3DSprite::WriteObject(const std::string& name, const char* key, double value)
{
    EditDataInObjectDataOperation(T(name), T(key), std::round(value * 10000) / 10000, u"set"_s);
}

const char* const* Glue3DSprite::EditedKeys()
{
    if (CurrentSceneTool == SceneTool::Rotate) return RotateKeys;
    if (CurrentSceneTool == SceneTool::Scale) return ScaleKeys;
    return MoveKeys;
}

// ---------------- toolbar ----------------

/// Toolbar layout: with the gizmo on, Select / Move / Rotate / Scale and then the gizmo button.
/// With the gizmo off, the modes are hidden and the gizmo button takes Select's place.
static bool ButtonVisible(int i, bool gizmoOn) { return gizmoOn || i == GizmoButton; }

void Glue3DSprite::ButtonPos(int i, double& x, double& y)
{
    int slot = ShowGizmo ? i : 0;
    x = ViewLeft() + 5 + slot * (ButtonSize + ButtonGap) + (ShowGizmo && i == GizmoButton ? 6 : 0);
    y = ViewTop() - 5;
}

int Glue3DSprite::ToolbarButtonAt(double mx, double my)
{
    for (int i = 0; i < 5; i++)
    {
        if (!ButtonVisible(i, ShowGizmo)) continue;
        double bx, by;
        ButtonPos(i, bx, by);
        if (mx >= bx && mx <= bx + ButtonSize && my <= by && my >= by - ButtonSize) return i;
    }
    return -1;
}

// ---------------- picking and handles ----------------

std::string Glue3DSprite::PickObject(double mx, double my)
{
    std::string best;
    double bestDist = 10;
    for (const SceneObject& o : SceneObjects())
    {
        for (double yOff : {0.0, 0.25}) // the object's pivot and its icon (drawn a bit higher)
        {
            Vec2 p;
            double depth;
            if (!Project(o.X, o.Y + yOff, o.Z, p, depth)) continue;
            double d = std::sqrt((p.x - mx) * (p.x - mx) + (p.y - my) * (p.y - my));
            if (d < bestDist) { bestDist = d; best = o.Name; }
        }
    }
    return best;
}

bool Glue3DSprite::SelectedPivot(Vec3& pivot, Vec2& screen, double& arm)
{
    pivot = {};
    screen = {};
    arm = 0;
    if (selected_.empty()) return false;
    pivot = {(float)ReadObject(selected_, "x"), (float)ReadObject(selected_, "y"), (float)ReadObject(selected_, "z")};
    double depth;
    if (!Project(pivot.x, pivot.y, pivot.z, screen, depth)) return false;
    arm = ArmLength(depth);
    return true;
}

/// Which gizmo handle is under the mouse (0-2 = X/Y/Z, 3 = center), or -1.
int Glue3DSprite::HandleAt(double mx, double my)
{
    if (!ShowGizmo || CurrentSceneTool == SceneTool::Select) return -1;
    Vec3 pivot;
    Vec2 center;
    double arm;
    if (!SelectedPivot(pivot, center, arm)) return -1;
    Vec2 m{(float)mx, (float)my};
    int best = -1;
    double bestDist = 6;
    if (CurrentSceneTool == SceneTool::Rotate)
    {
        for (int i = 0; i < 3; i++)
        {
            bool havePrev = false;
            Vec2 prev;
            for (int k = 0; k <= 48; k++)
            {
                Vec2 p;
                if (!ProjectV(pivot + RingPoint(i, k * kPi / 24) * (float)arm, p)) { havePrev = false; continue; }
                if (havePrev)
                {
                    double d = DistToSegment(m, prev, p);
                    if (d < bestDist) { bestDist = d; best = i; }
                }
                prev = p;
                havePrev = true;
            }
        }
        return best;
    }
    if (Vec2::Distance(m, center) < 7) return CenterHandle;
    for (int i = 0; i < 3; i++)
    {
        Vec2 end;
        if (!ProjectV(pivot + Axis(i) * (float)arm, end)) continue;
        double d = DistToSegment(m, center, end);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

// ---------------- per-frame update (before scripts run) ----------------

void Glue3DSprite::SceneGizmoUpdate()
{
    auto& input = GetEngine().Input;
    bool down = input.MouseDown;
    bool pressed = down && !prevMouseDown_;
    prevMouseDown_ = down;
    sceneViewActive_ = SceneViewActive();
    if (!sceneViewActive_)
    {
        dragHandle_ = -1;
        hotHandle_ = -1;
        hoverButton_ = -1;
        if (consumingClick_ && down) input.MouseDown = false;
        else consumingClick_ = false;
        return;
    }
    if (!selected_.empty() && stage.scene.IndexOf(T(selected_ + ".start")) == 0) selected_.clear(); // deleted

    double mx = MouseX(), my = MouseY();
    hoverButton_ = ToolbarButtonAt(mx, my);

    if (pressed && hoverButton_ >= 0)
    {
        if (hoverButton_ == GizmoButton)
        {
            ShowGizmo = !ShowGizmo;
            if (!ShowGizmo)
            {
                // Off means off: nothing selected, nothing dragged; clicks go to Glue3D (its object menu).
                selected_.clear();
                dragHandle_ = hotHandle_ = -1;
            }
        }
        else CurrentSceneTool = (SceneTool)hoverButton_;
        hoverButton_ = ToolbarButtonAt(mx, my); // the layout may have changed
        StartSound(u"Windows Vista Beta Blip 1"_s);
        consumingClick_ = true;
    }
    else if (pressed && ShowGizmo && CurrentSceneTool != SceneTool::Select && InSceneViewport(mx, my) && !RightMouseDown())
    {
        // Glue3D's own click handling (the object menu) is used in Select mode and whenever the gizmo is off.
        consumingClick_ = true;
        int handle = HandleAt(mx, my);
        if (handle >= 0) BeginDrag(handle, mx, my);
        else
        {
            std::string picked = PickObject(mx, my);
            if (picked != selected_ && !picked.empty()) StartSound(u"Windows Navigation Start3"_s);
            selected_ = picked;
        }
    }

    if (dragHandle_ >= 0 && down) UpdateDrag(mx, my);
    if (!down)
    {
        dragHandle_ = -1;
        consumingClick_ = false;
    }
    else if (consumingClick_) input.MouseDown = false; // hide this click from the Scratch scripts

    hotHandle_ = dragHandle_ >= 0 ? dragHandle_ : (down ? -1 : HandleAt(mx, my));
}

void Glue3DSprite::BeginDrag(int handle, double mx, double my)
{
    Vec3 pivot;
    Vec2 center;
    double arm;
    if (selected_.empty() || !SelectedPivot(pivot, center, arm)) return;
    dragHandle_ = handle;
    dragStartMouseX_ = mx;
    dragStartMouseY_ = my;
    dragArm_ = arm;
    const char* const* keys = EditedKeys();
    for (int i = 0; i < 3; i++) dragStart_[i] = ReadObject(selected_, keys[i]);

    bool planeMove = handle == CenterHandle && CurrentSceneTool == SceneTool::Move;
    double px, pz;
    if (planeMove && UnprojectToPlane(mx, my, pivot.y, px, pz)) { dragPlaneStartX_ = px; dragPlaneStartZ_ = pz; }
    else if (planeMove) dragHandle_ = -1;

    Vec2 end;
    if (handle < 3 && CurrentSceneTool != SceneTool::Rotate && ProjectV(pivot + Axis(handle) * (float)arm, end))
        dragAxisScreen_ = end - center;

    if (handle < 3 && CurrentSceneTool == SceneTool::Rotate)
    {
        // Tangent of the ring where it was grabbed, so dragging along the ring turns it.
        Vec2 m{(float)mx, (float)my};
        double bestA = 0, bestD = 1e300;
        for (int k = 0; k < 96; k++)
        {
            double a = k * kPi / 48;
            Vec2 p;
            if (ProjectV(pivot + RingPoint(handle, a) * (float)arm, p) && Vec2::Distance(p, m) < bestD)
            {
                bestD = Vec2::Distance(p, m);
                bestA = a;
            }
        }
        Vec2 p0, p1;
        ProjectV(pivot + RingPoint(handle, bestA) * (float)arm, p0);
        ProjectV(pivot + RingPoint(handle, bestA + 0.05) * (float)arm, p1);
        Vec2 t = p1 - p0;
        ringTangent_ = t.LengthSquared() > 1e-8f ? t.Normalized() : Vec2(1, 0);
        ringRadius_ = std::max(8.0f, Vec2::Distance(p0, center));
    }
}

void Glue3DSprite::UpdateDrag(double mx, double my)
{
    if (selected_.empty()) return;
    auto& input = GetEngine().Input;
    bool snap = input.IsPhysicalKeyDown(scratch::Key::LeftControl) || input.IsPhysicalKeyDown(scratch::Key::RightControl);
    const char* const* keys = EditedKeys();
    double dx = mx - dragStartMouseX_, dy = my - dragStartMouseY_;
    Vec2 mouseDelta{(float)dx, (float)dy};

    switch (CurrentSceneTool)
    {
    case SceneTool::Move:
        if (dragHandle_ < 3)
        {
            double along = AlongAxis(mouseDelta);
            WriteObject(selected_, keys[dragHandle_], Snap(dragStart_[dragHandle_] + along, MoveSnap, snap));
        }
        else // the center handle: slide on the ground
        {
            double px, pz;
            if (!UnprojectToPlane(mx, my, dragStart_[1], px, pz)) break;
            WriteObject(selected_, "x", Snap(dragStart_[0] + px - dragPlaneStartX_, MoveSnap, snap));
            WriteObject(selected_, "z", Snap(dragStart_[2] + pz - dragPlaneStartZ_, MoveSnap, snap));
        }
        break;
    case SceneTool::Rotate:
        if (dragHandle_ < 3)
        {
            double degrees = Vec2::Dot(mouseDelta, ringTangent_) / ringRadius_ * 180 / kPi;
            WriteObject(selected_, keys[dragHandle_], WrapAngle(Snap(dragStart_[dragHandle_] + degrees, RotateSnap, snap)));
        }
        else // (not used: rotate has no center handle) drag left/right to turn around the vertical axis
            WriteObject(selected_, "rx", WrapAngle(Snap(dragStart_[1] + dx * 1.5, RotateSnap, snap)));
        break;
    case SceneTool::Scale:
        if (dragHandle_ < 3)
        {
            double factor = std::max(0.01, 1 + AlongAxis(mouseDelta) / std::max(1e-6, dragArm_));
            WriteObject(selected_, keys[dragHandle_], std::max(0.01, Snap(dragStart_[dragHandle_] * factor, ScaleSnap, snap)));
        }
        else // the center handle: uniform scale
        {
            double factor = std::max(0.01, 1 + (dx + dy) / 60);
            for (int i = 0; i < 3; i++) WriteObject(selected_, keys[i], std::max(0.01, Snap(dragStart_[i] * factor, ScaleSnap, snap)));
        }
        break;
    default: break;
    }
}

/// World distance along the dragged axis for a mouse movement (stage units).
double Glue3DSprite::AlongAxis(Vec2 mouseDelta)
{
    float len2 = dragAxisScreen_.LengthSquared();
    if (len2 < 1e-6f) return 0;
    return Vec2::Dot(mouseDelta, dragAxisScreen_) / len2 * dragArm_;
}

// ---------------- drawing (after the stage is drawn) ----------------

static Vec2 ToScreen(Rect r, Vec2 s)
{
    float k = r.w / 480.0f;
    return {r.x + (s.x + 240) * k, r.y + (180 - s.y) * k};
}

void Glue3DSprite::SceneGizmoDraw(Rect stageRect)
{
    if (!sceneViewActive_) return;
    float k = stageRect.w / 480.0f;

    // Keep everything inside the 3D view.
    Vec2 tl = ToScreen(stageRect, {(float)ViewLeft(), (float)ViewTop()});
    Vec2 br = ToScreen(stageRect, {(float)ViewRight(), (float)ViewBottom()});
    gfx::Scissor({tl.x, tl.y, br.x - tl.x, br.y - tl.y});

    Vec3 pivot;
    Vec2 center;
    double arm;
    if (ShowGizmo && !selected_.empty() && SelectedPivot(pivot, center, arm))
    {
        Vec2 c = ToScreen(stageRect, center);
        gfx::DrawCircleLines(c, 9 * k, {255, 214, 0, 200});
        if (ShowGizmo && CurrentSceneTool != SceneTool::Select) DrawGizmo(pivot, arm, k, stageRect);
        DrawLabel(selected_, c + Vec2(11 * k, -16 * k), k);
        if (dragHandle_ >= 0) DrawReadout(c + Vec2(11 * k, 4 * k), k);
    }

    DrawToolbar(k, stageRect);
    gfx::NoScissor();
}

void Glue3DSprite::DrawGizmo(Vec3 pivot, double arm, float k, Rect stageRect)
{
    Vec2 c0;
    if (!ProjectV(pivot, c0)) return;
    Vec2 center = ToScreen(stageRect, c0);
    float thick = 2.2f * k;

    if (CurrentSceneTool == SceneTool::Rotate)
    {
        for (int i = 0; i < 3; i++)
        {
            Color col = i == hotHandle_ ? HotColor : AxisColors[i];
            bool havePrev = false;
            Vec2 prev;
            for (int s = 0; s <= 64; s++)
            {
                Vec2 p;
                if (!ProjectV(pivot + RingPoint(i, s * kPi / 32) * (float)arm, p)) { havePrev = false; continue; }
                Vec2 sp = ToScreen(stageRect, p);
                if (havePrev) gfx::DrawLine(prev, sp, thick, col);
                prev = sp;
                havePrev = true;
            }
        }
        gfx::DrawCircle(center, 2.5f * k, Color::White());
        return;
    }

    for (int i = 0; i < 3; i++)
    {
        Vec2 e;
        if (!ProjectV(pivot + Axis(i) * (float)arm, e)) continue;
        Vec2 end = ToScreen(stageRect, e);
        Color col = i == hotHandle_ ? HotColor : AxisColors[i];
        gfx::DrawLine(center, end, thick, col);
        Vec2 dir = end - center;
        if (dir.LengthSquared() < 1) continue;
        dir = dir.Normalized();
        Vec2 side{-dir.y, dir.x};
        if (CurrentSceneTool == SceneTool::Move)
        {
            // arrow head
            Vec2 tip = end + dir * (7 * k), l = end + side * (3.5f * k), r = end - side * (3.5f * k);
            gfx::DrawTriangle(tip, l, r, col);
        }
        else
        {
            float h = 3.5f * k;
            gfx::DrawRect({end.x - h, end.y - h, 2 * h, 2 * h}, col);
        }
    }
    Color cc = hotHandle_ == CenterHandle ? HotColor : Color{235, 235, 235, 230};
    float hs = 4 * k;
    gfx::DrawRect({center.x - hs, center.y - hs, 2 * hs, 2 * hs}, cc);
    gfx::DrawRectLines({center.x - hs, center.y - hs, 2 * hs, 2 * hs}, 1, {40, 40, 40, 200});
}

void Glue3DSprite::DrawLabel(const std::string& text, Vec2 at, float k)
{
    float size = 9 * k;
    auto& mon = GetEngine().monitors;
    float w = mon.Measure(text, size);
    gfx::DrawRectRounded({at.x - 3 * k, at.y - 1 * k, w + 6 * k, size + 2 * k}, 0.4f, 4, {20, 20, 20, 170});
    mon.Text(text, at.x, at.y, size, Color::White());
}

void Glue3DSprite::DrawReadout(Vec2 at, float k)
{
    if (selected_.empty()) return;
    const char* const* keys = EditedKeys();
    std::vector<std::string> sorted(keys, keys + 3);
    std::sort(sorted.begin(), sorted.end()); // x y z / rx ry rz / sx sy sz
    std::string text;
    for (const std::string& key : sorted)
    {
        if (!text.empty()) text += "   ";
        text += key + " " + Short(ReadObject(selected_, key.c_str()));
    }
    DrawLabel(text, at, k);
}

static void DrawToolIcon(int i, Vec2 c, float s, bool active)
{
    Color col = active ? Color::White() : Color{215, 215, 215, 255};
    float t = std::max(1.5f, s * 0.16f);
    switch (i)
    {
    case 0: // pointer arrow
        gfx::DrawTriangle(c + Vec2(-s * 0.6f, -s), c + Vec2(-s * 0.6f, s * 0.6f), c + Vec2(s * 0.7f, s * 0.15f), col);
        gfx::DrawLine(c + Vec2(0, s * 0.2f), c + Vec2(s * 0.55f, s * 1.0f), t * 1.4f, col);
        break;
    case 1: // four-way arrows
        for (Vec2 d : {Vec2(1, 0), Vec2(-1, 0), Vec2(0, 1), Vec2(0, -1)})
        {
            Vec2 side{-d.y, d.x};
            gfx::DrawLine(c, c + d * (s * 0.75f), t, col);
            Vec2 tip = c + d * (s * 1.1f);
            gfx::DrawTriangle(tip, c + d * (s * 0.6f) + side * (s * 0.35f), c + d * (s * 0.6f) - side * (s * 0.35f), col);
        }
        break;
    case 2: // circular arrow
    {
        gfx::DrawRing(c, s * 0.75f, s * 0.75f + t, 40, 330, 24, col);
        float a = 3.14159265f * 40 / 180;
        Vec2 tipA = c + Vec2(std::cos(a), std::sin(a)) * (s * 0.85f);
        gfx::DrawTriangle(tipA + Vec2(s * 0.45f, -s * 0.05f), tipA + Vec2(-s * 0.2f, -s * 0.45f), tipA + Vec2(-s * 0.05f, s * 0.35f), col);
        break;
    }
    case 3: // box with a corner handle
        gfx::DrawRectLines({c.x - s * 0.9f, c.y - s * 0.3f, s * 1.2f, s * 1.2f}, t, col);
        gfx::DrawLine(c + Vec2(-s * 0.1f, s * 0.1f), c + Vec2(s * 0.9f, -s * 0.9f), t, col);
        gfx::DrawRect({c.x + s * 0.5f, c.y - s * 1.1f, s * 0.6f, s * 0.6f}, col);
        break;
    case 4: // XYZ axes
        gfx::DrawLine(c + Vec2(-s * 0.6f, s * 0.6f), c + Vec2(s * 0.9f, s * 0.6f), t, AxisColors[0]);
        gfx::DrawLine(c + Vec2(-s * 0.6f, s * 0.6f), c + Vec2(-s * 0.6f, -s * 0.95f), t, AxisColors[1]);
        gfx::DrawLine(c + Vec2(-s * 0.6f, s * 0.6f), c + Vec2(s * 0.4f, -s * 0.35f), t, AxisColors[2]);
        if (!active) gfx::DrawLine(c + Vec2(-s, s), c + Vec2(s, -s), t * 1.2f, {240, 90, 90, 255});
        break;
    }
}

void Glue3DSprite::DrawToolbar(float k, Rect stageRect)
{
    for (int i = 0; i < 5; i++)
    {
        if (!ButtonVisible(i, ShowGizmo)) continue;
        double bx, by;
        ButtonPos(i, bx, by);
        Vec2 p = ToScreen(stageRect, {(float)bx, (float)by});
        Rect r{p.x, p.y, (float)ButtonSize * k, (float)ButtonSize * k};
        bool active = i < 4 ? (int)CurrentSceneTool == i : ShowGizmo;
        Color bg = active ? Color{58, 121, 217, 235} : Color{38, 38, 42, 215};
        if (i == hoverButton_) bg = active ? Color{80, 140, 230, 245} : Color{70, 70, 76, 230};
        gfx::DrawRectRounded(r, 0.25f, 4, bg);
        gfx::DrawRectRoundedLines(r, 0.25f, 4, 1, {0, 0, 0, 120});
        DrawToolIcon(i, {r.x + r.w / 2, r.y + r.h / 2}, r.w * 0.32f, active);
    }
    if (hoverButton_ >= 0)
    {
        double bx, by;
        ButtonPos(hoverButton_, bx, by);
        Vec2 p = ToScreen(stageRect, {(float)bx, (float)(by - ButtonSize - 3)});
        DrawLabel(hoverButton_ == GizmoButton && !ShowGizmo ? GizmoOffTip : ButtonTips[hoverButton_], p, k);
    }
}

// ---------------- testing helpers ----------------

/// The object whose variables the editor shows while the game runs: the one selected in the scene.
std::string Glue3DSprite::InspectedObject() { return selected_; }
void Glue3DSprite::SelectForTest(const std::string& name) { selected_ = name; }

std::string Glue3DSprite::DescribeObject(const std::string& name)
{
    double x = ReadObject(name, "x"), y = ReadObject(name, "y"), z = ReadObject(name, "z");
    Vec2 p;
    double depth;
    std::string pos = Project(x, y, z, p, depth) ? Fmt(p.x, "%.1f") + " " + Fmt(p.y, "%.1f") : "offscreen";
    std::string ends;
    Vec3 pv;
    Vec2 c;
    double arm;
    if (selected_ == name && SelectedPivot(pv, c, arm))
        for (int i = 0; i < 3; i++)
        {
            Vec2 e;
            if (ProjectV(pv + Axis(i) * (float)arm, e)) ends += std::string(" ") + AxisNames[i] + "-end " + Fmt(e.x, "%.1f") + " " + Fmt(e.y, "%.1f") + ";";
        }
    auto& d = stage.ramEngineData;
    std::string cam = "cam " + Short(d.Item(112).Num()) + " " + Short(d.Item(114).Num()) + " " + Short(d.Item(116).Num()) + " yaw " +
                      Fmt(d.Item(118).Num(), "%.1f") + " pitch " + Fmt(d.Item(120).Num(), "%.1f") + "; menu " + d.Item(26).Str().ToUtf8();
    const char* tools[] = {"Select", "Move", "Rotate", "Scale"};
    std::string out = cam + "; " + name + ": screen " + pos + ";" + ends;
    for (const char* key : {"x", "y", "z", "rx", "ry", "rz", "sx", "sy", "sz"}) out += std::string(" ") + key + " " + Js::NumberToUtf8(ReadObject(name, key));
    out += std::string("; tool ") + tools[(int)CurrentSceneTool] + " hover " + std::to_string(hoverButton_) + " gizmo " + (ShowGizmo ? "True" : "False") +
           " selected " + selected_ + " active " + (sceneViewActive_ ? "True" : "False");
    return out;
}

void Glue3DSprite::SetCamera(double x, double y, double z, double yaw, double pitch)
{
    CreateAddress(u"engine.camera_x"_s, x, 56);
    CreateAddress(u"engine.camera_y"_s, y, 57);
    CreateAddress(u"engine.camera_z"_s, z, 58);
    CreateAddress(u"engine.camera_direction_x"_s, yaw, 59);
    CreateAddress(u"engine.camera_direction_y"_s, pitch, 60);
}

void Glue3DSprite::AddTestCamera(const std::string& name, double x, double y, double z)
{
    CreateObjectTypeModelOnRotationScript(T(name), u"3d"_s, u"camera"_s, x, y, z, 0, 0, 0, u""_s);
    stage.engineGamecamera = T(name);
}

void Glue3DSprite::AddTestObject(const std::string& name, double x, double y, double z)
{
    CreateObjectTypeModelOnRotationScript(T(name), u"3d"_s, u""_s, x, y, z, 0, 0, 0, u""_s);
}

} // namespace Glue3D
