#include "Engine.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "Automation.h"

namespace scratch {

Engine::Engine() : renderer(*this), AssetDir(PathJoin(ExeDir(), "assets")) {}

Engine::~Engine()
{
    threads_.clear();
    deadThreads_.clear();
    deadSprites_.clear();
}

// ---------------- setup ----------------

void Engine::SetStage(StageBase* stage)
{
    stage->engine = this;
    Stage = stage;
    owned_.emplace_back(stage);
}

void Engine::AddSprite(SpriteBase* s)
{
    s->engine = this;
    Sprites.push_back(s);
    owned_.emplace_back(s);
}

void Engine::AddClone(SpriteBase* original, SpriteBase* clone)
{
    // Clones go directly behind the sprite they were cloned from.
    auto it = std::find(Sprites.begin(), Sprites.end(), original);
    Sprites.insert(it == Sprites.end() ? Sprites.begin() : it, clone);
    for (const Hat& hat : clone->Hats())
        if (hat.Kind == HatKind::CloneStart) threads_.push_back(std::make_unique<ScriptThread>(clone, &hat));
}

void Engine::RemoveClone(SpriteBase* clone)
{
    clone->Disposed = true;
    Sprites.erase(std::remove(Sprites.begin(), Sprites.end(), clone), Sprites.end());
    for (auto& t : threads_)
        if (t->target == clone) t->Done = true;
    Audio.Release(*clone);
    deadSprites_.emplace_back(clone); // freed once no script of it is running
}

void Engine::CreateCloneOf(const String& spriteName)
{
    for (SpriteBase* s : Sprites)
        if (!s->IsClone && s->Name == spriteName) { s->CreateClone(); return; }
}

// ---------------- events ----------------

std::vector<Target*> Engine::ExecutionOrder() const
{
    std::vector<Target*> order;
    order.reserve(Sprites.size() + 1);
    for (auto it = Sprites.rbegin(); it != Sprites.rend(); ++it) order.push_back(*it);
    order.push_back(Stage);
    return order;
}

void Engine::StartHats(HatKind kind, const String& key, bool restartExisting)
{
    String lowerKey = key.Lower();
    for (Target* target : ExecutionOrder())
    {
        if (target->Disposed) continue;
        for (const Hat& hat : target->Hats())
        {
            if (hat.Kind != kind) continue;
            if (kind != HatKind::GreenFlag && hat.Key.Lower() != lowerKey) continue;
            auto existing = std::find_if(threads_.begin(), threads_.end(),
                                         [&](const auto& t) { return t->target == target && t->hat == &hat; });
            if (existing != threads_.end())
            {
                if (restartExisting)
                {
                    (*existing)->Done = true;
                    deadThreads_.push_back(std::move(*existing));
                    *existing = std::make_unique<ScriptThread>(target, &hat);
                }
                else if (!(*existing)->Done) continue;
                else threads_.push_back(std::make_unique<ScriptThread>(target, &hat));
            }
            else threads_.push_back(std::make_unique<ScriptThread>(target, &hat));
        }
    }
}

void Engine::KeyPressed(const String& scratchKey)
{
    StartHats(HatKind::KeyPressed, scratchKey, false);
    StartHats(HatKind::KeyPressed, u"any"_s, false);
}

void Engine::GreenFlag()
{
    StopAll();
    ResetTimer();
    for (Target* t : ExecutionOrder())
    {
        t->SoundPitch = 0;
        t->SoundPan = 0;
        Audio.Sync(*t);
    }
    StartHats(HatKind::GreenFlag, String(), true);
}

void Engine::StopAll()
{
    for (auto& t : threads_)
    {
        t->Done = true;
        deadThreads_.push_back(std::move(t));
    }
    threads_.clear();
    for (SpriteBase* s : std::vector<SpriteBase*>(Sprites))
        if (s->IsClone) RemoveClone(s);
    for (Target* t : ExecutionOrder()) t->ClearEffects();
    Audio.StopAll();
}

void Engine::StopOtherScripts(Target* target)
{
    for (auto& t : threads_)
        if (t->target == target && t.get() != current_) t->Done = true;
}

// ---------------- running scripts ----------------

void Engine::StepThreads()
{
    double t0 = Now();
    StepThreadsInner();
    ScriptSeconds += Now() - t0;
    // Nothing is running now, so ended scripts and deleted clones can be freed.
    deadThreads_.clear();
    deadSprites_.clear();
}

void Engine::StepThreadsInner()
{
    redrawRequested_ = false;
    double start = Now(), budget = 0.75 / FrameRate;
    while (true)
    {
        int active = 0;
        for (size_t i = 0; i < threads_.size(); i++)
        {
            ScriptThread* th = threads_[i].get();
            if (th->Done) continue;
            Step(*th);
            if (!th->Done) active++;
        }
        for (auto& t : threads_)
            if (t->Done) deadThreads_.push_back(std::move(t));
        threads_.erase(std::remove(threads_.begin(), threads_.end(), nullptr), threads_.end());
        if (active == 0 || redrawRequested_ || Now() - start >= budget) break;
    }
}

void Engine::Step(ScriptThread& th)
{
    current_ = &th;
    while (!th.Done && !th.stack.empty())
    {
        Script::Handle top = th.stack.back();
        top.resume();
        auto& p = top.promise();
        if (p.stopped) { th.Done = true; break; }
        if (p.error)
        {
            try { std::rethrow_exception(p.error); }
            catch (const std::exception& e) { std::fprintf(stderr, "[script error] %s\n", e.what()); }
            catch (...) { std::fprintf(stderr, "[script error]\n"); }
            th.Done = true;
            break;
        }
        if (top.done())
        {
            th.stack.pop_back(); // finished: continue the block that called it
            continue;
        }
        if (p.child)
        {
            Script::Handle c = p.child;
            p.child = {};
            th.stack.push_back(c); // entered a custom block that can pause: run it now
            continue;
        }
        current_ = nullptr;
        return; // paused until the next pass / frame
    }
    th.Done = true;
    current_ = nullptr;
}

// ---------------- window ----------------

static GLFWwindow* W(void* p) { return static_cast<GLFWwindow*>(p); }

void Engine::SetWindowTitle(const std::string& title)
{
    if (window_) glfwSetWindowTitle(W(window_), title.c_str());
}

void Engine::FitWindowToMonitor()
{
    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    if (!mon) return;
    const GLFWvidmode* mode = glfwGetVideoMode(mon);
    if (!mode || mode->width <= 0) return;
    int mx = 0, my = 0;
    glfwGetMonitorPos(mon, &mx, &my);
    // Largest whole multiple of 480x360 that fits comfortably on the screen.
    int scale = std::max(1, std::min((int)(mode->width * 0.9 / 480), (int)(mode->height * 0.85 / 360)));
    glfwSetWindowSize(W(window_), 480 * scale, 360 * scale);
    glfwSetWindowPos(W(window_), mx + (mode->width - 480 * scale) / 2, my + (mode->height - 360 * scale) / 2);
}

void Engine::ToggleFullscreen()
{
    GLFWwindow* win = W(window_);
    if (!fullscreen_)
    {
        glfwGetWindowPos(win, &windowedX_, &windowedY_);
        glfwGetWindowSize(win, &windowedW_, &windowedH_);
        GLFWmonitor* mon = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = mon ? glfwGetVideoMode(mon) : nullptr;
        if (!mode) return;
        int mx = 0, my = 0;
        glfwGetMonitorPos(mon, &mx, &my);
        // Borderless window covering the screen (alt-tab friendly, like raylib's borderless mode).
        glfwSetWindowAttrib(win, GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowMonitor(win, nullptr, mx, my, mode->width, mode->height, 0);
        fullscreen_ = true;
    }
    else
    {
        glfwSetWindowAttrib(win, GLFW_DECORATED, GLFW_TRUE);
        glfwSetWindowMonitor(win, nullptr, windowedX_, windowedY_, windowedW_, windowedH_, 0);
        fullscreen_ = false;
    }
}

Rect Engine::StageRect() const
{
    int w = 1, h = 1;
    if (window_) glfwGetFramebufferSize(W(window_), &w, &h);
    float scale = std::min(w / 480.0f, h / 360.0f);
    float sw = std::floor(480 * scale), sh = std::floor(360 * scale);
    return {std::floor((w - sw) / 2), std::floor((h - sh) / 2), sw, sh};
}

// ---------------- loading screen ----------------

/// Shows loading progress (at most ~30 times a second) and keeps the window responsive while loading.
void Engine::DrawLoading(bool force)
{
    double now = Now();
    if (!force && now - lastLoadingDraw_ < 1.0 / 30) return;
    lastLoadingDraw_ = now;
    auto* win = W(window_);
    glfwPollEvents();
    int fw, fh;
    glfwGetFramebufferSize(win, &fw, &fh);
    Rect stage = StageRect();
    std::string what = LoadProgress::Phase;
    gfx::BeginFrame(fw, fh);
    gfx::Clear({12, 14, 20, 255});
    if (LoadingScreen) LoadingScreen(*this, stage, LoadProgress::Fraction(), what);
    else
    {
        float k = stage.w / 480.0f;
        Rect bar{stage.x + stage.w / 2 - 120 * k, stage.y + stage.h / 2, 240 * k, 8 * k};
        gfx::DrawRectRounded(bar, 1, 8, {40, 44, 56, 255});
        gfx::DrawRectRounded({bar.x, bar.y, bar.w * LoadProgress::Fraction(), bar.h}, 1, 8, {80, 200, 240, 255});
        monitors.Text("loading " + what, bar.x, bar.y + 14 * k, 10 * k, {170, 180, 200, 255});
    }
    gfx::EndFrame();
    // Testing only: SCRATCH_LOADING_SHOT=<png> saves the loading screen once it is half way.
    static bool shot = false;
    if (const char* path = std::getenv("SCRATCH_LOADING_SHOT"); path && !shot && LoadProgress::Fraction() >= 0.5f)
    {
        shot = true;
        gfx::SaveScreenshot(path, fw, fh);
    }
    glfwSwapBuffers(win);
}

// ---------------- main loop ----------------

void Engine::Run(const std::string& title, const std::function<void(Engine&)>& createProject)
{
    if (!glfwInit())
    {
        std::fprintf(stderr, "could not start GLFW (no display?)\n");
        return;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* win = glfwCreateWindow(960, 720, title.c_str(), nullptr, nullptr);
    if (!win)
    {
        std::fprintf(stderr, "could not open a window with OpenGL 3.3\n");
        glfwTerminate();
        return;
    }
    window_ = win;
    glfwMakeContextCurrent(win);
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress))
    {
        std::fprintf(stderr, "could not load OpenGL\n");
        return;
    }
    glfwSwapInterval(0); // frame timing is done below (Scratch runs at a fixed 60 frames per second)
    FitWindowToMonitor();
    glfwShowWindow(win);
    if (StartFullscreen) ToggleFullscreen();
    Input.Attach(win);

    gfx::Init();
    monitors.Init(AssetDir);
    DrawLoading(true);
    LoadProgress::Done = 0;
    LoadProgress::OnStep = [this] { DrawLoading(false); };

    Audio.Init();
    {
        Rect r = StageRect();
        renderer.Init((int)r.w, (int)r.h);
    }
    createProject(*this);
    Audio.Preload(ExecutionOrder());
    LoadProgress::Phase = "ready";
    DrawLoading(true);
    LoadProgress::OnStep = nullptr;
    automation = Automation::FromEnvironment(*this);

    GreenFlag();
    const double frameTime = 1.0 / FrameRate;
    double nextFrame = Now();
    bool prevToggle = false;
    while (!glfwWindowShouldClose(win) && !(automation && automation->Finished))
    {
        glfwPollEvents();
        Rect stageRect = StageRect();
        Input.Update(*this, stageRect);
        if (automation) automation->Update();
        for (auto& f : BeforeScripts) f();
        bool toggle = Input.IsPhysicalKeyPressed(Key::F11) ||
                      (Input.IsPhysicalKeyPressed(Key::Enter) && (Input.IsPhysicalKeyDown(Key::LeftAlt) || Input.IsPhysicalKeyDown(Key::RightAlt)));
        if (toggle && !prevToggle) ToggleFullscreen();
        prevToggle = toggle;
        StepThreads();

        int fw, fh;
        glfwGetFramebufferSize(win, &fw, &fh);
        stageRect = StageRect();
        renderer.Present(stageRect, fw, fh);
        for (auto& f : Overlay) f(stageRect);
        monitors.Draw(stageRect);
        gfx::EndFrame();
        if (automation) automation->OnPresent(fw, fh);
        glfwSwapBuffers(win);

        // Wait for the next 1/60 s: sleep most of it, then spin for accuracy.
        nextFrame += frameTime;
        double now = Now();
        if (nextFrame < now - frameTime) nextFrame = now; // fell behind: don't try to catch up
        while ((now = Now()) < nextFrame)
        {
            double left = nextFrame - now;
            if (left > 0.002) std::this_thread::sleep_for(std::chrono::duration<double>(left - 0.0015));
        }
    }
    StopAll();
    deadThreads_.clear();
    Audio.Close();
    glfwDestroyWindow(win);
    window_ = nullptr;
    glfwTerminate();
}

} // namespace scratch
