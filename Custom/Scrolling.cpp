// Hand-written (not overwritten by the converter): mouse-wheel scrolling for Glue3D's scrollbars.
//
// Glue3D draws every scrollbar with its "scroller element" block, which stores the scroll position in the engine
// data at an address. Originally you had to drag the little handle. Now, while the mouse is on a scrollbar or
// near it (over the panel it scrolls), the mouse wheel moves it. If several are near, the closest one wins.
#include <algorithm>
#include <cmath>

#include "Engine.h"
#include "Glue3DSprite.g.h"

namespace Glue3D {

void Glue3DSprite::ScrollbarSeen(const Value& axis, const Value& pos, const Value& from, const Value& to, const Value& address,
                                 const Value& max)
{
    if (!Eq(stage.guiFreeze, 0) || !Eq(stage.ramEngineData.Item(26), 0)) return; // a dialog or menu is in front
    int addr = (int)address.Num();
    double mx = MouseX(), my = MouseY(), p = pos.Num();
    double lo = std::min(from.Num(), to.Num()), hi = std::max(from.Num(), to.Num());
    if (!Eq(axis, u"x"_s))
    {
        // A vertical bar sits at the right edge of what it scrolls: the mouse may be on it or to its left.
        if (my < lo - 6 || my > hi + 6 || mx > p + 8 || mx < p - ScrollReachVertical) return;
        // The script editor's own wheel scrolling already moves these two up/down: remember them, but don't move them.
        if (Eq(stage.editorTab, u"script"_s) && (addr == 87 || addr == 1)) addr = -1;
        double distance = std::fabs(p - mx);
        if (distance < scrollVertical_.Distance) scrollVertical_ = {distance, addr, max.Num(), true};
    }
    else
    {
        // A horizontal bar sits under what it scrolls: the mouse may be on it or above it.
        if (mx < lo - 10 || mx > hi + 10 || my < p - 8 || my > p + ScrollReachHorizontalShift) return;
        double distance = std::fabs(p - my);
        if (distance < scrollHorizontal_.Distance) scrollHorizontal_ = {distance, addr, max.Num(), my <= p + ScrollReachHorizontal};
    }
}

void Glue3DSprite::ScrollBy(const ScrollTarget& t, bool horizontal, double wheel)
{
    if (wheel == 0 || t.Address <= 0 || t.Max <= 0) return;
    double top = t.Max - (horizontal ? 2 : 1); // the same limits as dragging the handle
    double step = std::max(1.0, t.Max * 0.06);
    double value = stage.ramEngineData.Item(t.Address * 2).Num();
    value = std::clamp(value - wheel * step, 0.0, std::max(0.0, top)); // wheel up = towards the start
    EditAddress(t.Address, value);
}

void Glue3DSprite::ScrollWheelUpdate()
{
    // The bars drawn last frame; this frame's wheel.
    ScrollTarget vertical = scrollVertical_, horizontal = scrollHorizontal_;
    scrollVertical_ = {};
    scrollHorizontal_ = {};
    auto& input = GetEngine().Input;
    double up = input.WheelMove, side = input.WheelMoveX;
    if (input.ShiftHeld()) { side += up; up = 0; } // Shift + wheel: only sideways
    bool haveVertical = vertical.Distance < 1e300;
    bool haveHorizontal = horizontal.Distance < 1e300;

    // Up/down: the nearest vertical bar. Over something that only scrolls sideways (like the file strip), the
    // plain wheel moves that instead. Never both.
    if (up != 0)
    {
        if (haveVertical) ScrollBy(vertical, false, up);
        else if (haveHorizontal && horizontal.Near) ScrollBy(horizontal, true, up);
    }
    // Sideways (Shift + wheel, or a sideways wheel / touchpad): only the nearest horizontal bar.
    if (side != 0 && haveHorizontal) ScrollBy(horizontal, true, side);
}

} // namespace Glue3D
