// The Scratch runtime: owns the window, runs scripts frame by frame like Scratch's sequencer, and draws the stage.
#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Audio.h"
#include "Gfx.h"
#include "Input.h"
#include "Monitors.h"
#include "Renderer.h"
#include "Target.h"

namespace scratch {

class Automation;

/// One running script: the event script plus any custom blocks it is inside (innermost last).
struct ScriptThread
{
    Target* target;
    const Hat* hat;
    Script root;                                // owns the coroutine
    std::vector<Script::Handle> stack;          // root first
    bool Done = false;

    ScriptThread(Target* t, const Hat* h) : target(t), hat(h), root(h->Start(t)) { stack.push_back(root.handle()); }
};

class Engine
{
public:
    static constexpr double FrameRate = 60; // from the project's TurboWarp settings

    StageBase* Stage = nullptr;
    /// Sprites and clones, bottom layer first.
    std::vector<SpriteBase*> Sprites;
    Renderer renderer;
    AudioEngine Audio;
    InputState Input;
    Monitors monitors;
    std::unique_ptr<Automation> automation;

    /// Folder with costumes, sounds and fonts (a built game keeps them in data/assets).
    std::string AssetDir;
    /// Open in borderless fullscreen (F11 toggles).
    bool StartFullscreen = false;

    /// Runs every frame after input is read and before scripts run (hand-written tools hook in here).
    std::vector<std::function<void()>> BeforeScripts;
    /// Draws the loading screen while the project starts (progress 0..1 and what is loading). The default
    /// is a plain progress bar; the app can set its own.
    std::function<void(Engine&, Rect stage, float progress, const std::string& what)> LoadingScreen;
    /// Draws on top of the stage every frame, in window pixels (the stage rectangle is passed in).
    std::vector<std::function<void(Rect)>> Overlay;

    Engine();
    ~Engine();

    double Now() const { return std::chrono::duration<double>(std::chrono::steady_clock::now() - clockStart_).count(); }
    double Timer() const { return Now() - timerStart_; }
    void ResetTimer() { timerStart_ = Now(); }
    void RequestRedraw() { redrawRequested_ = true; }

    // ---- setup ----
    void SetStage(StageBase* stage);
    void AddSprite(SpriteBase* s);
    void AddClone(SpriteBase* original, SpriteBase* clone);
    void RemoveClone(SpriteBase* clone);
    void CreateCloneOf(const String& spriteName);

    // ---- events ----
    void Broadcast(const String& name) { StartHats(HatKind::Broadcast, name, true); }
    void KeyPressed(const String& scratchKey);
    void GreenFlag();
    void StopAll();
    void StopOtherScripts(Target* target);

    // ---- window ----
    void SetWindowTitle(const std::string& title);
    void ToggleFullscreen();
    /// Every target, top sprite first, the stage last (Scratch's execution order).
    std::vector<Target*> ExecutionOrder() const;

    /// Total time spent running scripts (for the automation "stats" command).
    double ScriptSeconds = 0;

    /// Opens the window and runs until it is closed. createProject builds the stage and sprites.
    void Run(const std::string& title, const std::function<void(Engine&)>& createProject);

    /// The 4:3 stage area inside the window (letterboxed), in framebuffer pixels.
    Rect StageRect() const;
    void* Window() const { return window_; }

private:
    std::chrono::steady_clock::time_point clockStart_ = std::chrono::steady_clock::now();
    double timerStart_ = 0;
    bool redrawRequested_ = false;
    std::vector<std::unique_ptr<ScriptThread>> threads_;
    ScriptThread* current_ = nullptr;
    /// Threads and clones that ended while a script was running; freed at a safe point.
    std::vector<std::unique_ptr<ScriptThread>> deadThreads_;
    std::vector<std::unique_ptr<SpriteBase>> deadSprites_;
    std::vector<std::unique_ptr<Target>> owned_;
    void* window_ = nullptr;
    int windowedX_ = 0, windowedY_ = 0, windowedW_ = 960, windowedH_ = 720;
    bool fullscreen_ = false;

    void StartHats(HatKind kind, const String& key, bool restartExisting);
    void StepThreads();
    void StepThreadsInner();
    void Step(ScriptThread& th);
    void FitWindowToMonitor();
    void DrawLoading(bool force);
    double lastLoadingDraw_ = -1;
};

/// Joins a frame's worth of the stage rectangle math: stage units -> window pixels.
inline float StageToWindowX(const Rect& r, double x) { return (float)(r.x + (x + 240) * r.w / 480); }
inline float StageToWindowY(const Rect& r, double y) { return (float)(r.y + (180 - y) * r.h / 360); }

} // namespace scratch
