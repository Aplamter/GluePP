// Scratch scripts as C++20 coroutines.
//
//   Script WhenGreenFlagClicked()
//   {
//       while (true)
//       {
//           Broadcast(u"Tick"_s);
//           co_yield NextFrame;        // Scratch waits for the next frame here
//       }
//   }
//
// A custom block that can pause is a Script too, and is called with co_await:
//   co_await MyBlock(1, 2);
// The engine keeps a stack of running blocks per script (like Scratch's thread stack) and resumes the
// innermost one each frame.
#pragma once
#include <coroutine>
#include <exception>
#include <utility>

namespace scratch {

/// Thrown to end the current script immediately (e.g. "delete this clone").
struct StopScript
{
};

struct NextFrameT
{
};
/// co_yield NextFrame;  pauses the script until the next frame.
inline constexpr NextFrameT NextFrame{};

class Script
{
public:
    struct promise_type
    {
        /// Set while this script is waiting for a custom block it called with co_await.
        std::coroutine_handle<promise_type> child;
        bool stopped = false;               // "stop this script" from a nested call / delete this clone
        std::exception_ptr error;

        Script get_return_object() { return Script(std::coroutine_handle<promise_type>::from_promise(*this)); }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        std::suspend_always yield_value(NextFrameT) noexcept { return {}; }
        void unhandled_exception() noexcept
        {
            try { throw; }
            catch (const StopScript&) { stopped = true; }
            catch (...) { error = std::current_exception(); }
        }
    };
    using Handle = std::coroutine_handle<promise_type>;

    Script() = default;
    explicit Script(Handle h) : h_(h) {}
    Script(Script&& o) noexcept : h_(std::exchange(o.h_, {})) {}
    Script& operator=(Script&& o) noexcept
    {
        if (this != &o) { if (h_) h_.destroy(); h_ = std::exchange(o.h_, {}); }
        return *this;
    }
    Script(const Script&) = delete;
    Script& operator=(const Script&) = delete;
    ~Script() { if (h_) h_.destroy(); }

    Handle handle() const { return h_; }
    Handle release() { return std::exchange(h_, {}); }

    // co_await child: the engine runs the child right away and comes back here when it finishes.
    bool await_ready() const noexcept { return false; }
    void await_suspend(Handle parent) noexcept { parent.promise().child = h_; }
    void await_resume() const noexcept {}

private:
    Handle h_;
};

} // namespace scratch
