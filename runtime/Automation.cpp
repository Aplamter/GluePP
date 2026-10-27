#include "Automation.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "Engine.h"

namespace scratch {

std::map<std::string, std::function<void(const std::string&)>>& Automation::Hooks()
{
    static std::map<std::string, std::function<void(const std::string&)>> hooks;
    return hooks;
}

std::unique_ptr<Automation> Automation::FromEnvironment(Engine& engine)
{
    const char* path = std::getenv("SCRATCH_AUTOMATION");
    if (!path || !*path) return nullptr;
    std::string text;
    if (!ReadFile(path, text)) return nullptr;
    std::deque<std::string> lines;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);)
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return std::unique_ptr<Automation>(new Automation(engine, std::move(lines)));
}

Automation::Automation(Engine& e, std::deque<std::string> lines) : engine_(e), commands_(std::move(lines))
{
    engine_.Input.MouseOverride = InputState::MouseOverrideT{0, 0, false};
    statsStart_ = engine_.Now();
}

static std::string Trim(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

void Automation::Update()
{
    frames_++;
    while (pendingShot_.empty() && !commands_.empty() && engine_.Now() >= waitUntil_)
    {
        std::string line = Trim(commands_.front());
        commands_.pop_front();
        size_t sp = line.find(' ');
        std::string cmd = line.substr(0, sp), arg = sp == std::string::npos ? "" : line.substr(sp + 1);
        std::istringstream a(arg);
        if (cmd == "wait") { double ms = 0; a >> ms; waitUntil_ = engine_.Now() + ms / 1000; }
        else if (cmd == "move") a >> mx_ >> my_;
        else if (cmd == "down") down_ = true;
        else if (cmd == "up") down_ = false;
        else if (cmd == "key") engine_.Input.KeyDown(engine_, String::FromUtf8(arg));
        else if (cmd == "keyup") engine_.Input.KeyUp(String::FromUtf8(arg));
        else if (cmd == "rdown") rdown_ = true;
        else if (cmd == "rup") rdown_ = false;
        else if (cmd == "look") { double dx, dy; a >> dx >> dy >> lookFrames_; if (lookFrames_ < 1) lookFrames_ = 1; ldx_ = dx / lookFrames_; ldy_ = dy / lookFrames_; }
        else if (cmd == "wheel") a >> wheel_;
        else if (cmd == "pan") { double dx, dy; a >> dx >> dy >> panFrames_; if (panFrames_ < 1) panFrames_ = 1; pdx_ = dx / panFrames_; pdy_ = dy / panFrames_; }
        else if (cmd == "shot") pendingShot_ = arg;
        else if (cmd == "stats")
        {
            double secs = engine_.Now() - statsStart_;
            std::printf("[automation] %d frames in %.1fs = %.1f fps; scripts %.2f ms/frame\n", frames_, secs, frames_ / secs,
                        1000 * engine_.ScriptSeconds / (frames_ > 0 ? frames_ : 1));
            std::fflush(stdout);
            engine_.ScriptSeconds = 0;
            frames_ = 0;
            statsStart_ = engine_.Now();
        }
        else if (cmd == "quit") Finished = true;
        else
        {
            auto it = Hooks().find(cmd);
            if (it != Hooks().end()) it->second(arg);
        }
    }
    engine_.Input.MouseOverride = InputState::MouseOverrideT{mx_, my_, down_};
    bool moving = lookFrames_ > 0;
    engine_.Input.LookOverride = InputState::LookOverrideT{rdown_, moving ? ldx_ : 0, moving ? ldy_ : 0};
    if (moving) lookFrames_--;
    engine_.Input.WheelMove = wheel_;
    engine_.Input.WheelMoveX = 0;
    wheel_ = 0;
    bool panning = panFrames_ > 0;
    engine_.Input.MiddleMouseDown = panning;
    engine_.Input.PanDeltaX = panning ? pdx_ : 0;
    engine_.Input.PanDeltaY = panning ? pdy_ : 0;
    if (panning) panFrames_--;
}

void Automation::OnPresent(int fbWidth, int fbHeight)
{
    if (pendingShot_.empty()) return;
    gfx::SaveScreenshot(pendingShot_, fbWidth, fbHeight);
    std::printf("[automation] saved %s\n", pendingShot_.c_str());
    std::fflush(stdout);
    pendingShot_.clear();
}

} // namespace scratch
