// On-stage list monitors ("show list"), drawn in Scratch's style. Also owns the UI font.
#pragma once
#include <string>
#include <vector>

#include "Gfx.h"
#include "ScratchList.h"

namespace scratch {

class Monitors
{
public:
    void Init(const std::string& assetDir);
    void AddList(ScratchList& list, double x, double y, double w, double h, bool visible);
    void SetVisible(ScratchList& list, bool visible);
    void Draw(Rect stage);

    /// Text in the UI font (Noto Sans), top-left at (x, y), in window pixels.
    void Text(const std::string& s, float x, float y, float size, Color c) const;
    float Measure(const std::string& s, float size) const;

private:
    struct ListMonitor
    {
        ScratchList* List;
        float X, Y, W, H;
        bool Visible;
    };
    std::vector<ListMonitor> lists_;
    Font font_;
};

} // namespace scratch
