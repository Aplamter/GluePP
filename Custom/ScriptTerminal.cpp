// Hand-written: the terminal tools (the editor and built games both run these).
//
//   watch / count / print_once / print_every   less spam: lines that update in place, or print once
//   show_source / timestamps / format / trace  who printed what, and when
//   info / success / error, filter, mute / only levels and filtering
//   assert / assert_pause / break / dump / print_list   catching bugs
//   time_start / time_end / fps_watch           timing
//   command / command_arg / input               the dev console (press ` while the game runs)
//   overlay / log_to_file / max_lines           the terminal in built games
//
// Every terminal line goes through TerminalHook (tools/glue3d_edits.py hooks it into Glue3D's TerminalPrint),
// which adds the source and time, filters, writes the log file and keeps the terminal short.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#include <GLFW/glfw3.h>

#include "BuildFlavor.h"
#include "Engine.h"
#include "Glue3DSprite.g.h"
#include "Platform.h"

namespace Glue3D {

using namespace scratch;

namespace {

Value T(const std::string& s) { return Value(String::FromUtf8(s)); }
std::string U(const Value& v) { return v.Str().ToUtf8(); }
Value Bool(bool b) { return b ? u"true"_s : u"false"_s; }

std::string Spaced(std::string s)
{
    std::replace(s.begin(), s.end(), '_', ' ');
    return s;
}

std::string Trim(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// The levels, and the terminal color (Scratch color effect on the text) each one prints in.
enum Level { Info = 0, Success = 1, Warn = 2, Error = 3 };
constexpr double LevelColor[] = {15, 60, 25, -10};
const char* LevelName[] = {"info", "success", "warn", "error"};

int LevelFromName(const std::string& n)
{
    for (int i = 0; i < 4; i++)
        if (n == LevelName[i]) return i;
    if (n == "warning") return Warn;
    return Info;
}

double NowMs()
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

/// The script line running now (1 = the script's first line), or 0 outside a script.
int Glue3DSprite::ScriptLineNumber()
{
    if (Eq(stage.scriptName, u""_s)) return 0;
    return std::max(0, (int)(stage.scriptReadindex.Num() - stage.scriptHeader.Num()));
}

std::string Glue3DSprite::SourceText()
{
    return "[" + U(stage.scriptObject) + " :" + std::to_string(ScriptLineNumber()) + "]";
}

void Glue3DSprite::ResetTerminalTools()
{
    watches_.clear();
    printedOnce_.clear();
    printEvery_.clear();
    counts_.clear();
    timeStarts_.clear();
    showSource_ = timestamps_ = fpsWatch_ = false;
    filterLevel_ = 0;
    muted_.clear();
    only_.clear();
    breakpoint_ = false;
    inputAsker_.clear();
    inputAnswers_.clear();
    lastCommand_.clear();
}

/// Does this object have this name or tag?
bool Glue3DSprite::ObjectIs(const std::string& object, const std::string& nameOrTag)
{
    if (object == nameOrTag) return true;
    for (auto& o : ObjectRecords())
        if (o.Name == object) return U(RecordValue(o, u"tag")) == nameOrTag;
    return false;
}

// ------------------------------------------------------------------------------------------------
// Every line printed to the terminal comes here first (Glue3D's TerminalPrint calls it).
// ------------------------------------------------------------------------------------------------

bool Glue3DSprite::TerminalHook(const Value& message, const Value& color)
{
    std::string object = U(stage.scriptObject);
    std::string text = U(message);
    int level = printLevel_ >= 0 ? printLevel_ : (text.rfind("error", 0) == 0 ? Error : text.rfind("warning", 0) == 0 ? Warn : Info);
    printLevel_ = -1;

    // filters
    if (level < filterLevel_) return true;
    for (auto& m : muted_)
        if (ObjectIs(object, m)) return true;
    if (!only_.empty())
    {
        bool shown = false;
        for (auto& o : only_) shown = shown || ObjectIs(object, o);
        if (!shown) return true;
    }

    // who and when
    std::string prefix;
    if (timestamps_)
    {
        char t[32];
        std::snprintf(t, sizeof t, "[%.2fs] ", stage.allTime.Num());
        prefix += t;
    }
    if (showSource_) prefix += SourceText() + " ";
    std::string line = showSource_ ? prefix + text : object + ":" + prefix + text;
    stage.terminal.Add(T(line));
    stage.terminal.Add(color.Num() + 0);

    if (logToFile_)
    {
        std::string path = BuildFlavor::IsEditor && !ProjectFolder.empty() ? PathJoin(ProjectFolder, "log.txt") : PathJoin(ExeDir(), "log.txt");
        std::ofstream f(FsPath(path), std::ios::app | std::ios::binary); // UTF-8 paths work on Windows too
        if (f) f << line << "\n"; // the level is already in the text ("error: ...")
    }
    // keep the terminal short
    while (stage.terminal.Length() > maxLines_ * 2)
    {
        stage.terminal.Delete(1);
        stage.terminal.Delete(1);
    }
    return true;
}

void Glue3DSprite::PrintLevel(int level, const std::string& text)
{
    printLevel_ = level;
    TerminalPrint(T(text), LevelColor[level]);
    printLevel_ = -1;
}

// ------------------------------------------------------------------------------------------------
// The blocks
// ------------------------------------------------------------------------------------------------

bool Glue3DSprite::TerminalBlocksMore()
{
    String c = Cmd();
    std::string me = U(stage.scriptObject);
    std::string here = me + "@" + U(stage.scriptName) + "#" + std::to_string((int)stage.scriptReadindex.Num());

    // ---- less spam ----
    if (c == u"watch")
    {
        // watch(str hp, var hp): one line that updates in place
        SetWatch(Spaced(U(Arg(0))), U(Arg(1)));
        return true;
    }
    if (c == u"unwatch")
    {
        std::string label = Spaced(U(Arg(0)));
        watches_.erase(std::remove_if(watches_.begin(), watches_.end(), [&](auto& w) { return w.first == label; }), watches_.end());
        counts_.erase(label);
        return true;
    }
    if (c == u"count")
    {
        std::string label = Spaced(U(Arg(0)));
        SetWatch(label, std::to_string(++counts_[label]));
        return true;
    }
    if (c == u"print_once")
    {
        if (printedOnce_.insert(here).second) PrintLevel(Info, Spaced(U(Arg(0))));
        return true;
    }
    if (c == u"print_every")
    {
        // print_every(val 1, var speed): at most once per that many seconds
        double secs = Arg(0).Num(), now = GetEngine().Now();
        auto it = printEvery_.find(here);
        if (it == printEvery_.end() || now - it->second >= secs)
        {
            printEvery_[here] = now;
            std::string value = U(Arg(1));
            if (Eq(ArgType(1), u"var"_s)) value = U(stage.decodedElements.Item(7)) + ": " + value;
            else value = Spaced(value);
            PrintLevel(Info, value);
        }
        return true;
    }

    // ---- who and when ----
    if (c == u"show_source") return showSource_ = !Eq(Arg(0), u"false"_s), true;
    if (c == u"timestamps") return timestamps_ = !Eq(Arg(0), u"false"_s), true;
    if (c == u"format")
    {
        // format(str hp={}_ammo={}, var hp, var ammo): each {} becomes the next value
        std::string pattern = Spaced(U(Arg(0))), out;
        int next = 1;
        for (size_t i = 0; i < pattern.size(); i++)
            if (pattern.compare(i, 2, "{}") == 0)
            {
                out += U(Arg(next++));
                i++;
            }
            else
                out += pattern[i];
        PrintLevel(Info, out);
        return true;
    }
    if (c == u"trace")
    {
        std::string text = "trace " + SourceText() + " in script " + U(stage.scriptName);
        auto it = callStacks_.find(me + "@" + U(stage.scriptName));
        if (it != callStacks_.end() && !it->second.empty())
        {
            text += ", called from line";
            for (auto r = it->second.rbegin(); r != it->second.rend(); ++r)
                text += " " + std::to_string((int)(*r - stage.scriptHeader.Num())) + (std::next(r) == it->second.rend() ? "" : " <");
        }
        PrintLevel(Info, text);
        return true;
    }

    // ---- levels and filtering ----
    if (c == u"info" || c == u"success" || c == u"error")
    {
        int level = c == u"info" ? Info : c == u"success" ? Success : Error;
        PrintLevel(level, (level == Info ? "" : std::string(LevelName[level]) + ": ") + Spaced(U(Arg(0))));
        return true;
    }
    if (c == u"filter") return filterLevel_ = LevelFromName(U(Arg(0))), true;
    if (c == u"mute") return muted_.insert(U(Arg(0))), true;
    if (c == u"unmute") return muted_.erase(U(Arg(0))), true;
    if (c == u"only")
    {
        std::string who = U(Arg(0));
        if (who.empty() || who == "all") only_.clear();
        else only_.insert(who);
        return true;
    }
    if (c == u"show_all")
    {
        only_.clear();
        muted_.clear();
        filterLevel_ = 0;
        return true;
    }

    // ---- catching bugs ----
    if (c == u"assert" || c == u"assert_pause")
    {
        // assert(var hp >= val 0, str hp_went_negative): the condition is like if's; the last value is the message
        std::string line = U(stage.scriptLine);
        size_t a = line.find('('), b = line.rfind(')');
        std::string inside = a != std::string::npos && b != std::string::npos && b > a ? line.substr(a + 1, b - a - 1) : "";
        size_t comma = inside.rfind(',');
        std::string cond = Trim(comma == std::string::npos ? inside : inside.substr(0, comma));
        std::string msg = comma == std::string::npos ? "assert failed" : Trim(inside.substr(comma + 1));
        if (msg.rfind("str ", 0) == 0) msg = Spaced(Trim(msg.substr(4)));
        else if (msg.rfind("var ", 0) == 0) msg = U(ReadData(stage.scriptObject, T(Trim(msg.substr(4)))));
        bool ok = false;
        if (!EvalConditionText(cond, ok))
        {
            PrintLevel(Error, "error: " + SourceText() + " assert can't read its condition");
            return true;
        }
        if (!ok)
        {
            PrintLevel(Error, "error: " + SourceText() + " assert failed: " + msg);
            if (c == u"assert_pause") Breakpoint("assert failed");
        }
        return true;
    }
    if (c == u"break")
    {
        Breakpoint("break");
        return true;
    }
    if (c == u"dump")
    {
        Value who = Eq(ArgType(0), u""_s) ? stage.scriptObject : Arg(0);
        DumpObject(U(who));
        return true;
    }
    if (c == u"print_list")
    {
        Value name = Arg(0);
        std::vector<std::string> items;
        if (!ListRead(name, items)) return true;
        PrintLevel(Info, U(name) + " (" + std::to_string(items.size()) + " items)");
        for (size_t i = 0; i < items.size(); i++) PrintLevel(Info, "  " + std::to_string(i + 1) + ": " + items[i]);
        return true;
    }

    // ---- timing ----
    if (c == u"time_start") return timeStarts_[me + "#" + U(Arg(0))] = NowMs(), true;
    if (c == u"time_end")
    {
        std::string name = U(Arg(0));
        auto it = timeStarts_.find(me + "#" + name);
        if (it == timeStarts_.end())
        {
            PrintLevel(Warn, "warning: time_end(" + name + ") without time_start");
            return true;
        }
        char t[64];
        std::snprintf(t, sizeof t, "%s took %.3f ms", Spaced(name).c_str(), NowMs() - it->second);
        timeStarts_.erase(it);
        PrintLevel(Info, t);
        return true;
    }
    if (c == u"fps_watch") return fpsWatch_ = !Eq(Arg(0), u"false"_s), true;

    // ---- the dev console ----
    if (c == u"command")
    {
        SetReturn(Bool(TakeEvent("\x01" "cmd:" + U(Arg(0)))));
        return true;
    }
    if (c == u"command_arg")
    {
        int n = (int)Arg(0).Num();
        SetReturn(n >= 0 && n < (int)lastCommand_.size() ? T(lastCommand_[n]) : u""_s);
        return true;
    }
    if (c == u"input")
    {
        // input(str name?): asks in the console and waits for the answer (the answer goes into data_return)
        auto it = inputAnswers_.find(me);
        if (it != inputAnswers_.end())
        {
            SetReturn(T(it->second));
            inputAnswers_.erase(it);
            stage.scene.Replace(stage.objectIndex.Num() + 35, 0);
            return true;
        }
        if (inputAsker_.empty())
        {
            inputAsker_ = me;
            inputPrompt_ = Spaced(U(Arg(0)));
            consoleOpen_ = true;
            consoleLine_.clear();
        }
        stage.scene.Replace(stage.objectIndex.Num() + 35, 1); // wait here for the answer
        return true;
    }

    // ---- built games ----
    if (c == u"overlay") return overlay_ = !Eq(Arg(0), u"false"_s), true;
    if (c == u"log_to_file") return logToFile_ = !Eq(Arg(0), u"false"_s), true;
    if (c == u"max_lines")
    {
        maxLines_ = std::max(1.0, Arg(0).Num());
        return true;
    }
    return false;
}

void Glue3DSprite::SetWatch(const std::string& label, const std::string& value)
{
    for (auto& w : watches_)
        if (w.first == label)
        {
            w.second = value;
            return;
        }
    watches_.push_back({label, value});
}

void Glue3DSprite::DumpObject(const std::string& name)
{
    for (auto& o : ObjectRecords())
        if (o.Name == name)
        {
            PrintLevel(Info, "variables of " + name + ":");
            for (int i = o.Index + 2; i + 1 < o.Index + o.Length; i += 2)
                PrintLevel(Info, "  " + U(stage.scene.Item(i)) + " = " + U(stage.scene.Item(i + 1)));
            return;
        }
    PrintLevel(Warn, "warning: no object \"" + name + "\" to dump");
}

/// Stops every script (until F8, or "continue" in the console) and prints this object's variables.
void Glue3DSprite::Breakpoint(const std::string& why)
{
    PrintLevel(Warn, "warning: " + why + " at " + SourceText() + " - everything is paused, press F8 to continue");
    DumpObject(U(stage.scriptObject));
    breakpoint_ = true;
}

// ------------------------------------------------------------------------------------------------
// Every frame: the console, F8, the fps line
// ------------------------------------------------------------------------------------------------

bool Glue3DSprite::GameRunning() { return Eq(stage.allRun, 1) || !BuildFlavor::IsEditor; }

void Glue3DSprite::TerminalUpdate()
{
    auto& input = GetEngine().Input;
    if (breakpoint_ && input.IsPhysicalKeyPressed(GLFW_KEY_F8)) breakpoint_ = false;
    if (fpsWatch_)
    {
        double d = stage.timeDelta.Num() / std::max(1e-9, TimeScale());
        SetWatch("fps", std::to_string(d > 0 ? (int)std::lround(1 / d) : 0) + "   objects: " + std::to_string(ObjectRecords().size()));
    }
    if (!GameRunning())
    {
        consoleOpen_ = false;
        input.TextCapture = false;
        return;
    }
    if (!consoleOpen_)
    {
        input.TextCapture = false;
        if (input.IsPhysicalKeyPressed(GLFW_KEY_GRAVE_ACCENT))
        {
            consoleOpen_ = true;
            consoleLine_.clear();
        }
        return;
    }
    // typing into the console (the game doesn't see these keys)
    bool wasCapturing = input.TextCapture;
    input.TextCapture = true;
    if (!wasCapturing)
    {
        input.Typed.clear();
        input.TypedKeys.clear();
        return;
    }
    for (char ch : input.Typed)
        if (ch != '`' && ch != '~' && (unsigned char)ch >= 32) consoleLine_ += ch;
    input.Typed.clear();
    std::vector<int> keys;
    keys.swap(input.TypedKeys);
    for (int k : keys)
    {
        if (k == GLFW_KEY_BACKSPACE && !consoleLine_.empty())
        {
            // remove one whole UTF-8 character
            size_t n = consoleLine_.size() - 1;
            while (n > 0 && ((unsigned char)consoleLine_[n] & 0xC0) == 0x80) n--;
            consoleLine_.erase(n);
        }
        else if (k == GLFW_KEY_ENTER || k == GLFW_KEY_KP_ENTER)
        {
            std::string text = consoleLine_;
            consoleLine_.clear();
            SubmitConsole(text);
        }
        else if ((k == GLFW_KEY_ESCAPE || k == GLFW_KEY_GRAVE_ACCENT) && inputAsker_.empty())
        {
            consoleOpen_ = false;
            input.TextCapture = false;
            break;
        }
    }
}

/// A line typed in the console: an answer to terminal.input, or a command for terminal.command.
void Glue3DSprite::SubmitConsole(const std::string& raw)
{
    std::string text = Trim(raw);
    Value saved = stage.scriptObject;
    stage.scriptObject = u"console"_s;
    PrintLevel(Info, "> " + text);
    stage.scriptObject = saved;
    if (!inputAsker_.empty())
    {
        inputAnswers_[inputAsker_] = text;
        inputAsker_.clear();
        inputPrompt_.clear();
        consoleOpen_ = false;
        GetEngine().Input.TextCapture = false;
        return;
    }
    if (text.empty()) return;
    if (text == "continue" && breakpoint_)
    {
        breakpoint_ = false;
        return;
    }
    std::istringstream in(text);
    lastCommand_.clear();
    for (std::string w; in >> w;) lastCommand_.push_back(w);
    for (auto& o : ObjectRecords()) SendEvent(o.Name, "\x01" "cmd:" + lastCommand_[0]);
}

// ------------------------------------------------------------------------------------------------
// Drawing: watched lines (top-left of the game view), the terminal overlay and console (bottom-left)
// ------------------------------------------------------------------------------------------------

void Glue3DSprite::DrawTerminalOverlay(Rect r)
{
    float k = r.w / 480.0f;
    auto& mon = GetEngine().monitors;
    Rect view{StageToWindowX(r, stage.engineScreenTopleftx.Num()), StageToWindowY(r, stage.engineScreenToplefty.Num()), 0, 0};
    view.w = StageToWindowX(r, stage.engineScreenBottomrightx.Num()) - view.x;
    view.h = StageToWindowY(r, stage.engineScreenBottomrighty.Num()) - view.y;
    if (view.w <= 0 || view.h <= 0) return;
    DrawVariablesPanel(r, view); // the selected object's variables (Custom/ScriptVariables.cpp)
    float size = 8.5f * k, lineH = size + 3 * k, pad = 5 * k;

    // watched lines
    if (!watches_.empty())
    {
        float w = 0;
        std::vector<std::string> lines;
        for (auto& [label, value] : watches_)
        {
            lines.push_back(label + ": " + value);
            w = std::max(w, mon.Measure(lines.back(), size));
        }
        Rect box{view.x + pad, view.y + pad, w + 2 * pad, lines.size() * lineH + pad};
        gfx::DrawRectRounded(box, 0.15f, 6, {12, 14, 20, 175});
        for (size_t i = 0; i < lines.size(); i++)
            mon.Text(lines[i], box.x + pad, box.y + pad / 2 + i * lineH, size, {150, 230, 255, 255});
    }

    // the terminal overlay and console
    bool showLines = overlay_ || consoleOpen_;
    if (!showLines || !GameRunning()) return;
    int count = consoleOpen_ ? 10 : 6;
    std::vector<std::pair<std::string, Color>> lines;
    int items = (int)stage.terminal.Length() / 2;
    for (int i = std::max(0, items - count); i < items; i++)
    {
        std::string text = U(stage.terminal.Item(i * 2 + 1));
        double col = stage.terminal.Item(i * 2 + 2).Num();
        Color c{225, 225, 225, 255};
        if (col == LevelColor[Success]) c = {120, 235, 120, 255};
        else if (col == LevelColor[Warn]) c = {255, 215, 90, 255};
        else if (col == LevelColor[Error]) c = {255, 110, 100, 255};
        lines.push_back({text, c});
    }
    float boxH = (lines.size() + (consoleOpen_ ? 1 : 0)) * lineH + pad;
    if (boxH <= pad) return;
    float boxW = std::min(view.w - 2 * pad, 300 * k);
    Rect box{view.x + pad, view.y + view.h - pad - boxH, boxW, boxH};
    gfx::DrawRectRounded(box, 0.08f, 6, {8, 10, 14, consoleOpen_ ? (uint8_t)215 : (uint8_t)150});
    gfx::Scissor(box);
    float y = box.y + pad / 2;
    for (auto& [text, c] : lines)
    {
        mon.Text(text, box.x + pad, y, size, c);
        y += lineH;
    }
    if (consoleOpen_)
    {
        std::string prompt = inputPrompt_.empty() ? "> " : inputPrompt_ + " ";
        bool blink = std::fmod(GetEngine().Now(), 1.0) < 0.5;
        mon.Text(prompt + consoleLine_ + (blink ? "_" : ""), box.x + pad, y, size, {255, 255, 255, 255});
    }
    gfx::NoScissor();
}

} // namespace Glue3D
