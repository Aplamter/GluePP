// Optional scripted input for automated testing. Set SCRATCH_AUTOMATION to a file of commands:
//   wait <ms> | move <x> <y> | down | up | key <scratch key> | keyup <key> | rdown | rup | look <dx> <dy> <frames>
//   | wheel <n> | pan <dx> <dy> <frames> | shot <png path> | stats | quit
// Coordinates are Scratch stage coordinates. Not used in normal play.
#pragma once
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace scratch {

class Engine;

class Automation
{
public:
    static std::unique_ptr<Automation> FromEnvironment(Engine& engine);
    void Update();
    /// Called right before the frame is shown, so screenshots contain the finished frame.
    void OnPresent(int fbWidth, int fbHeight);
    bool Finished = false;
    /// Extra test commands registered by the app ("name args...").
    static std::map<std::string, std::function<void(const std::string&)>>& Hooks();

private:
    Automation(Engine& e, std::deque<std::string> lines);
    Engine& engine_;
    std::deque<std::string> commands_;
    double waitUntil_ = 0;
    std::string pendingShot_;
    double mx_ = 0, my_ = 0;
    bool down_ = false, rdown_ = false;
    double ldx_ = 0, ldy_ = 0, wheel_ = 0, pdx_ = 0, pdy_ = 0;
    int lookFrames_ = 0, panFrames_ = 0;
    int frames_ = 0;
    double statsStart_ = 0;
};

} // namespace scratch
