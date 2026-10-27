#include "Monitors.h"

#include <algorithm>

#include "Assets.h"

namespace scratch {

void Monitors::Init(const std::string& assetDir)
{
    font_ = gfx::LoadFont(PathJoin(assetDir, "fonts/NotoSans-Medium.ttf"), 48);
}

void Monitors::AddList(ScratchList& list, double x, double y, double w, double h, bool visible)
{
    lists_.push_back({&list, (float)x, (float)y, (float)(w > 0 ? w : 100), (float)(h > 0 ? h : 200), visible});
}

void Monitors::SetVisible(ScratchList& list, bool visible)
{
    for (auto& m : lists_)
        if (m.List == &list) { m.Visible = visible; return; }
    if (visible) lists_.push_back({&list, 5, 5, 100, 200, true});
}

void Monitors::Text(const std::string& s, float x, float y, float size, Color c) const { gfx::DrawText(font_, s, x, y, size, c); }

float Monitors::Measure(const std::string& s, float size) const { return gfx::MeasureText(font_, s, size); }

void Monitors::Draw(Rect stage)
{
    float k = stage.w / 480.0f;
    Color border{195, 204, 217, 255}, bg{229, 240, 255, 255}, item{252, 102, 44, 255}, text{87, 94, 117, 255};
    for (auto& m : lists_)
    {
        if (!m.Visible) continue;
        Rect r{stage.x + m.X * k, stage.y + m.Y * k, m.W * k, m.H * k};
        gfx::Scissor(stage);
        gfx::DrawRectRounded(r, 0.04f, 6, bg);
        gfx::DrawRectRoundedLines(r, 0.04f, 6, 1 * k, border);
        float fs = 12 * k, rowH = 22 * k, header = 22 * k, footer = 22 * k;
        std::string title = m.List->Name.ToUtf8();
        gfx::DrawRect({r.x + 1, r.y + header, r.w - 2, 1}, border);
        Text(title, r.x + (r.w - Measure(title, fs)) / 2, r.y + (header - fs) / 2, fs, text);
        const auto& items = m.List->Items;
        float top = r.y + header + 2 * k, bottom = r.y + r.h - footer;
        gfx::Scissor({r.x, top, r.w, std::max(0.0f, bottom - top)});
        for (size_t i = 0; i < items.size(); i++)
        {
            float y = top + i * rowH;
            if (y > bottom) break;
            std::string n = std::to_string(i + 1);
            Text(n, r.x + 4 * k, y + (rowH - fs) / 2, fs, text);
            float bx = r.x + (8 + 8 * n.size()) * k;
            Rect box{bx, y + 2 * k, r.x + r.w - 6 * k - bx, rowH - 4 * k};
            gfx::DrawRectRounded(box, 0.25f, 4, item);
            Text(items[i].Str().ToUtf8(), box.x + 4 * k, y + (rowH - fs) / 2, fs, Color::White());
        }
        gfx::Scissor(stage);
        std::string len = "length " + std::to_string(items.size());
        gfx::DrawRect({r.x + 1, bottom, r.w - 2, 1}, border);
        Text(len, r.x + (r.w - Measure(len, fs)) / 2, bottom + (footer - fs) / 2, fs, text);
        gfx::NoScissor();
    }
}

} // namespace scratch
