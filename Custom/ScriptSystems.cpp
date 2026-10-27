// Hand-written: the bigger script features (the editor and built games both run these):
//   events between objects, functions, object collision and raycasts, single-press keys, tags,
//   saved progress, levels (load_scene / restart / pause / time scale), clone helpers and cooldowns,
//   inline answers, and/or conditions, text helpers, camera shake, gamepads, 3D sound, UI styles and
//   bars, keyframe animation, and debug drawing.
// Custom/ScriptBlocks.cpp sends every script line here first (SystemsCommand).
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <random>
#include <regex>
#include <sstream>

#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>

#include "BuildFlavor.h"
#include "Engine.h"
#include "Glue3DSprite.g.h"
#include "Platform.h"
#include "ProjectFiles.h"

namespace Glue3D {

using namespace scratch;

namespace {

Value T(const std::string& s) { return Value(String::FromUtf8(s)); }
std::string U(const Value& v) { return v.Str().ToUtf8(); }
Value Bool(bool b) { return b ? u"true"_s : u"false"_s; }

std::string Trim(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

/// The words of a line before its "(" ("call respawn (val 1)" -> {"call", "respawn"}).
std::vector<std::string> HeadWords(const std::string& line)
{
    std::istringstream in(line.substr(0, line.find('(')));
    std::vector<std::string> out;
    for (std::string w; in >> w;) out.push_back(w);
    return out;
}

} // namespace

// ------------------------------------------------------------------------------------------------
// Scene helpers
// ------------------------------------------------------------------------------------------------

/// Every object in the scene: its name and where its record starts in stage.scene.
std::vector<Glue3DSprite::ObjectRecord> Glue3DSprite::ObjectRecords()
{
    std::vector<ObjectRecord> out;
    int idx = 2;
    int n = (int)stage.scene.Item(1).Num();
    for (int i = 0; i < n; i++)
    {
        int len = (int)stage.scene.Item(idx).Num();
        if (len < 3) break;
        out.push_back({U(stage.scene.Item(idx + 3)), idx, len});
        idx += len;
    }
    return out;
}

/// A value from an object's record (`fallback` when it doesn't have that variable).
Value Glue3DSprite::RecordValue(const ObjectRecord& o, const char16_t* key, Value fallback)
{
    std::u16string_view k(key);
    for (int i = o.Index + 2; i + 1 < o.Index + o.Length; i += 2)
        if (stage.scene.Item(i).Str().view() == k) return stage.scene.Item(i + 1);
    return fallback;
}

/// Cameras and UI elements aren't things you can bump into.
bool Glue3DSprite::IsSolid(const ObjectRecord& o)
{
    Value m = RecordValue(o, u"model");
    return !Eq(m, u"camera"_s) && !Eq(m, u"elementui"_s);
}

/// Does this object have that name, or that tag?
bool Glue3DSprite::Matches(const ObjectRecord& o, const std::string& nameOrTag)
{
    return o.Name == nameOrTag || U(RecordValue(o, u"tag")) == nameOrTag;
}

/// An object's collision box: centre and full size. The size is its physics hitbox (1 if it has none) times
/// its scale.
void Glue3DSprite::Box(const ObjectRecord& o, double c[3], double s[3])
{
    const char16_t* pos[] = {u"x", u"y", u"z"};
    const char16_t* hit[] = {u"hitbox_x", u"hitbox_y", u"hitbox_z"};
    const char16_t* scl[] = {u"sx", u"sy", u"sz"};
    for (int i = 0; i < 3; i++)
    {
        c[i] = RecordValue(o, pos[i]).Num();
        s[i] = std::fabs(RecordValue(o, hit[i], 1).Num() * RecordValue(o, scl[i], 1).Num());
    }
}

bool Glue3DSprite::BoxesTouch(const ObjectRecord& a, const ObjectRecord& b)
{
    double ca[3], sa[3], cb[3], sb[3];
    Box(a, ca, sa);
    Box(b, cb, sb);
    for (int i = 0; i < 3; i++)
        if (std::fabs(ca[i] - cb[i]) > (sa[i] + sb[i]) / 2) return false;
    return true;
}

const Glue3DSprite::ObjectRecord* Glue3DSprite::FindObject(const std::vector<ObjectRecord>& all, const std::string& name)
{
    for (auto& o : all)
        if (o.Name == name) return &o;
    return nullptr;
}

/// Starting a game (or running it again) starts with no events, calls, key presses or cooldowns left over.
void Glue3DSprite::ResetIfNewRun()
{
    double clock = stage.allClock.Num();
    if (clock < lastRunClock_)
    {
        events_.clear();
        callStacks_.clear();
        keySeen_.clear();
        cooldowns_.clear();
        loopCounters_.clear();
        paused_ = false;
        pauseExempt_.clear();
        timeScale_ = 1;
        ResetTerminalTools();
        ResetVariableTools();
    }
    lastRunClock_ = clock;
}

/// Every frame, before scripts (from GameControlsUpdate): counts key presses and releases.
void Glue3DSprite::ScriptBlocksUpdate()
{
    LevelUpdate();
    TerminalUpdate();
    VariablesUpdate();
    ShakeUpdate();
    AnimationUpdate();
    for (auto& [key, k] : keys_)
    {
        bool down = IsInputDown(key);
        if (down && !k.Down) k.Presses++;
        if (!down && k.Down) k.Releases++;
        k.Down = down;
    }
}

/// Key names as scripts write them: "up" or "up_arrow" means Scratch's "up arrow".
static std::string KeyName(std::string k)
{
    std::replace(k.begin(), k.end(), '_', ' ');
    if (k == "up" || k == "down" || k == "left" || k == "right") k += " arrow";
    return k;
}

/// True once per press (or release) of a key, for each object that asks.
bool Glue3DSprite::KeyEdge(const std::string& rawKey, bool release)
{
    std::string key = rawKey == "mouse" || rawKey.rfind("pad:", 0) == 0 ? rawKey : KeyName(rawKey);
    auto [it, isNew] = keys_.try_emplace(key);
    KeyState& k = it->second;
    if (isNew) k.Down = IsInputDown(key);
    long count = release ? k.Releases : k.Presses;
    std::string id = U(stage.scriptObject) + (release ? "^" : "v") + key;
    auto seen = keySeen_.find(id);
    if (seen == keySeen_.end())
    {
        keySeen_[id] = count; // the first question starts counting from now
        return false;
    }
    if (count > seen->second)
    {
        seen->second = count;
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------------------------------
// The blocks
// ------------------------------------------------------------------------------------------------

bool Glue3DSprite::SystemsCommand()
{
    ResetIfNewRun();
    String kind = stage.decodedElements.Item(1).Str();
    if (kind == u"var" || kind == u"global" || kind == u"local" || kind == u"const" || kind == u"saved")
        return AssignCommand(); // Custom/ScriptVariables.cpp
    if (kind == u"terminal" && TerminalBlocksMore()) return true;
    if (kind == u"math" && TextBlocks()) return true;
    if (kind == u"event") return EventBlocks();
    if (kind == u"func" || kind == u"endfunc" || kind == u"call" || kind == u"return") return FunctionBlocks();
    if (kind == u"physics") return CollisionBlocks();
    if (kind == u"game" && KeyEventBlocks()) return true;
    if (kind == u"game" && GamepadBlocks()) return true;
    if (kind == u"object") return TagBlocks() || CloneBlocks();
    if (kind == u"save") return SaveBlocks();
    if (kind == u"debug") return DebugBlocks();
    if (kind == u"sound" && SoundAtBlocks()) return true;
    if (kind == u"interface" && UiBlocks()) return true;
    if (kind == u"model" && AnimationBlocks()) return true;
    if (kind == u"game" && (PolishGameBlocks())) return true;
    if (kind == u"time") return TimeScaleBlocks();
    if (kind == u"game") return LevelBlocks();
    return false;
}

// ---- events ----
void Glue3DSprite::SendEvent(const std::string& object, const std::string& event) { events_[object].insert(event); }

bool Glue3DSprite::TakeEvent(const std::string& event)
{
    auto it = events_.find(U(stage.scriptObject));
    if (it == events_.end()) return false;
    return it->second.erase(event) > 0;
}

bool Glue3DSprite::EventBlocks()
{
    String c = Cmd();
    if (c == u"broadcast")
    {
        std::string e = U(Arg(0));
        for (auto& o : ObjectRecords()) SendEvent(o.Name, e);
        return true;
    }
    if (c == u"send")
    {
        // send(str object, str event): one object (or every object with that tag)
        std::string to = U(Arg(0)), e = U(Arg(1));
        auto all = ObjectRecords();
        for (auto& o : all)
            if (Matches(o, to)) SendEvent(o.Name, e);
        return true;
    }
    if (c == u"received")
    {
        SetReturn(Bool(TakeEvent(U(Arg(0)))));
        return true;
    }
    if (c == u"wait_for")
    {
        // The program counter only moves on while the object's wait value (+35) is about 0.
        stage.scene.Replace(stage.objectIndex.Num() + 35, TakeEvent(U(Arg(0))) ? 0 : 1);
        return true;
    }
    if (c == u"clear")
    {
        events_.erase(U(stage.scriptObject));
        return true;
    }
    return false;
}

// ---- functions: func name … endfunc, call name (args), return (value) ----
int Glue3DSprite::FindFunction(const std::string& name)
{
    int end = (int)stage.loadedScripts.IndexOf(stage.scriptName.Str() + u".end"_s);
    for (int i = (int)stage.scriptHeader.Num() + 1; i < end; i++)
    {
        auto words = HeadWords(U(stage.loadedScripts.Item(i)));
        if (words.size() >= 2 && words[0] == "func" && words[1] == name) return i;
    }
    return 0;
}

/// The values in a line's parentheses, each "type value" (var is read, data_return too).
std::vector<Value> Glue3DSprite::LineArgs(const std::string& line)
{
    std::vector<Value> out;
    size_t a = line.find('('), b = line.rfind(')');
    if (a == std::string::npos || b == std::string::npos || b <= a) return out;
    std::string inside = line.substr(a + 1, b - a - 1);
    // split at the commas that aren't inside brackets
    std::vector<std::string> parts;
    std::string cur;
    int depth = 0;
    for (char ch : inside)
    {
        if (ch == '(' || ch == '[') depth++;
        if (ch == ')' || ch == ']') depth--;
        if (ch == ',' && depth == 0)
        {
            parts.push_back(cur);
            cur.clear();
        }
        else
            cur += ch;
    }
    parts.push_back(cur);
    for (auto& part : parts)
    {
        part = Trim(part);
        if (part.empty()) continue;
        // any value or math: val 1, var hp, global score, var hp * val 2, object.distance(str player)…
        Value v;
        bool waiting = false;
        if (EvalExpression(part, v, waiting)) out.push_back(v);
        else out.push_back(T(part.substr(part.find(' ') == std::string::npos ? 0 : part.find(' ') + 1)));
    }
    return out;
}

bool Glue3DSprite::FunctionBlocks()
{
    std::string w = U(stage.decodedElements.Item(1));
    std::string line = U(stage.scriptLine);
    int here = (int)stage.scriptReadindex.Num();
    std::string key = U(stage.scriptObject) + "@" + U(stage.scriptName);
    if (w == "func")
    {
        // Reached by running down the script (not by call): skip the function's body.
        int end = FindBlockEnd(here, "func", "endfunc");
        if (end) JumpTo(end + 1);
        else TerminalPrint(u"error: func without endfunc!"_s, u""_s);
        return true;
    }
    if (w == "call")
    {
        auto words = HeadWords(line);
        std::string name = words.size() >= 2 ? words[1] : "";
        int at = FindFunction(name);
        if (!at)
        {
            TerminalPrint(T("error: no function \"" + name + "\" in this script"), u""_s);
            return true;
        }
        auto& stack = callStacks_[key];
        if (stack.size() >= 200)
        {
            TerminalPrint(u"error: too many calls inside calls (endless recursion?)"_s, u""_s);
            return true;
        }
        auto args = LineArgs(line);
        // arg1, arg2… (and "local" variables) belong to this call: they get their old values back when it returns
        PushFrame();
        for (size_t i = 0; i < args.size(); i++)
        {
            std::string arg = "arg" + std::to_string(i + 1);
            MakeLocal(arg);
            WriteData(stage.scriptObject, T(arg), args[i]);
        }
        stack.push_back(here);
        JumpTo(at + 1);
        return true;
    }
    if (w == "return" || w == "endfunc")
    {
        if (w == "return")
        {
            auto args = LineArgs(line);
            if (!args.empty()) SetReturn(args[0]);
        }
        auto it = callStacks_.find(key);
        if (it == callStacks_.end() || it->second.empty())
        {
            if (w == "return") TerminalPrint(u"error: return outside a function"_s, u""_s);
            return true;
        }
        int back = it->second.back();
        it->second.pop_back();
        PopFrame();
        JumpTo(back + 1);
        return true;
    }
    return false;
}

// ---- collision: touching, touching_any, raycast ----
bool Glue3DSprite::CollisionBlocks()
{
    String c = Cmd();
    if (c != u"touching" && c != u"touching_any" && c != u"raycast") return false;
    auto all = ObjectRecords();
    const ObjectRecord* me = FindObject(all, U(stage.scriptObject));
    if (!me) return true;
    if (c == u"touching")
    {
        std::string what = U(Arg(0));
        bool hit = false;
        for (auto& o : all)
            if (&o != me && IsSolid(o) && Matches(o, what) && BoxesTouch(*me, o))
            {
                hit = true;
                break;
            }
        SetReturn(Bool(hit));
        return true;
    }
    if (c == u"touching_any")
    {
        std::string hit;
        for (auto& o : all)
            if (&o != me && IsSolid(o) && BoxesTouch(*me, o))
            {
                hit = o.Name;
                break;
            }
        SetReturn(T(hit));
        return true;
    }
    // raycast(val dir, val pitch, val distance): the first object along that line, from this object's centre.
    double dir = Arg(0).Num(), pitch = Arg(1).Num(), dist = Arg(2).Num();
    double p[3] = {RecordValue(*me, u"x").Num(), RecordValue(*me, u"y").Num(), RecordValue(*me, u"z").Num()};
    // the same directions move_dir and game.movement use: x = cos(dir + 90), z = sin(dir + 90)
    double cp = Cos(pitch);
    double d[3] = {Cos(dir + 90) * cp, Sin(pitch), Sin(dir + 90) * cp};
    std::string best;
    double bestT = dist;
    for (auto& o : all)
    {
        if (&o == me || !IsSolid(o)) continue;
        double cc[3], s[3];
        Box(o, cc, s);
        // ray vs box (slab method)
        double t0 = 0, t1 = bestT;
        bool miss = false;
        for (int i = 0; i < 3 && !miss; i++)
        {
            double lo = cc[i] - s[i] / 2, hi = cc[i] + s[i] / 2;
            if (std::fabs(d[i]) < 1e-12)
            {
                if (p[i] < lo || p[i] > hi) miss = true;
                continue;
            }
            double ta = (lo - p[i]) / d[i], tb = (hi - p[i]) / d[i];
            if (ta > tb) std::swap(ta, tb);
            t0 = std::max(t0, ta);
            t1 = std::min(t1, tb);
            if (t0 > t1) miss = true;
        }
        if (!miss && t0 <= bestT)
        {
            bestT = t0;
            best = o.Name;
        }
    }
    SetReturn(T(best));
    WriteData(stage.scriptObject, u"ray_distance"_s, best.empty() ? -1 : bestT);
    return true;
}

// ---- single key presses ----
bool Glue3DSprite::KeyEventBlocks()
{
    String c = Cmd();
    if (c == u"key_down") return SetReturn(Bool(KeyEdge(U(Arg(0)), false))), true;
    if (c == u"key_up") return SetReturn(Bool(KeyEdge(U(Arg(0)), true))), true;
    if (c == u"mouse_clicked") return SetReturn(Bool(KeyEdge("mouse", false))), true;
    return false;
}

// ---- tags ----
bool Glue3DSprite::TagBlocks()
{
    String c = Cmd();
    if (c == u"set_tag")
    {
        WriteData(stage.scriptObject, u"tag"_s, Arg(0));
        return true;
    }
    if (c == u"get_tag")
    {
        Value t = ReadData(stage.scriptObject, u"tag"_s);
        SetReturn(Eq(t, u"null"_s) ? u""_s : t);
        return true;
    }
    if (c == u"nearest" || c == u"count" || c == u"broadcast_tag")
    {
        std::string tag = U(Arg(0));
        Value event = Arg(1);
        auto all = ObjectRecords();
        const ObjectRecord* me = FindObject(all, U(stage.scriptObject));
        if (c == u"count")
        {
            int n = 0;
            for (auto& o : all)
                if (U(RecordValue(o, u"tag")) == tag) n++;
            SetReturn(n);
        }
        else if (c == u"broadcast_tag")
        {
            for (auto& o : all)
                if (U(RecordValue(o, u"tag")) == tag) SendEvent(o.Name, U(event));
        }
        else
        {
            double mx = me ? RecordValue(*me, u"x").Num() : 0, my = me ? RecordValue(*me, u"y").Num() : 0,
                   mz = me ? RecordValue(*me, u"z").Num() : 0;
            std::string best;
            double bestD = 1e300;
            for (auto& o : all)
            {
                if (&o == me || U(RecordValue(o, u"tag")) != tag) continue;
                double dx = RecordValue(o, u"x").Num() - mx, dy = RecordValue(o, u"y").Num() - my, dz = RecordValue(o, u"z").Num() - mz;
                double d2 = dx * dx + dy * dy + dz * dz;
                if (d2 < bestD)
                {
                    bestD = d2;
                    best = o.Name;
                }
            }
            SetReturn(T(best));
        }
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------------------------------
// Saved progress: save.set / get / has / delete. A small JSON file that survives restarting the game:
// next to the game's .exe in a built game, in the project folder in the editor.
// ------------------------------------------------------------------------------------------------

std::string Glue3DSprite::SaveFilePath()
{
    if (BuildFlavor::IsEditor && !ProjectFolder.empty()) return PathJoin(ProjectFolder, "save.json");
    return PathJoin(ExeDir(), "save.json");
}

/// The save file's contents (read once per file).
static nlohmann::json& SaveData(const std::string& path)
{
    static nlohmann::json data;
    static std::string loadedFrom;
    if (path != loadedFrom)
    {
        loadedFrom = path;
        data = nlohmann::json::object();
        std::string text;
        if (ReadFile(path, text))
            try
            {
                data = nlohmann::json::parse(text);
                if (!data.is_object()) data = nlohmann::json::object();
            }
            catch (...)
            {
                data = nlohmann::json::object();
            }
    }
    return data;
}

void Glue3DSprite::WriteSaveFile()
{
    std::string text = SaveData(SaveFilePath()).dump(2);
    if (!WriteFileAtomic(SaveFilePath(), text)) TerminalPrint(u"warning: could not write the save file"_s, 25);
}

bool Glue3DSprite::SaveBlocks()
{
    String c = Cmd();
    auto& data = SaveData(SaveFilePath());
    std::string key = U(Arg(0));
    if (c == u"set")
    {
        Value v = Arg(1);
        // numbers stay numbers ("42" from val 42 too), everything else is text
        double d = v.Num();
        if (v.IsNumber() || (!U(v).empty() && U(Value(d)) == U(v)))
        {
            if (d == std::floor(d) && std::fabs(d) < 1e15) data[key] = (long long)d;
            else data[key] = d;
        }
        else data[key] = U(v);
        WriteSaveFile();
        return true;
    }
    if (c == u"get")
    {
        auto it = data.find(key);
        if (it == data.end()) SetReturn(Eq(ArgType(1), u""_s) ? Value(0) : Arg(1)); // get(str key, val default)
        else if (it->is_number()) SetReturn(it->get<double>());
        else if (it->is_string()) SetReturn(T(it->get<std::string>()));
        else SetReturn(T(it->dump()));
        return true;
    }
    if (c == u"has") return SetReturn(Bool(data.contains(key))), true;
    if (c == u"delete")
    {
        data.erase(key);
        WriteSaveFile();
        return true;
    }
    if (c == u"clear_all")
    {
        data = nlohmann::json::object();
        WriteSaveFile();
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------------------------------
// Levels: game.load_scene / restart / pause, time.scale
// ------------------------------------------------------------------------------------------------

double Glue3DSprite::TimeScale() { return Eq(stage.allRun, 1) ? timeScale_ : 1.0; }

bool Glue3DSprite::ScriptPaused()
{
    if (breakpoint_) return true; // terminal.break: everything waits
    if (!paused_) return false;
    if (Eq(stage.scene.Item(stage.objectIndex.Num() + 7), u"elementui"_s)) return false; // menus keep working
    return pauseExempt_.count(U(stage.scene.Item(stage.objectIndex.Num() + 3))) == 0;
}

bool Glue3DSprite::TimeScaleBlocks()
{
    String c = Cmd();
    if (c == u"scale")
    {
        timeScale_ = std::max(0.0, Arg(0).Num());
        return true;
    }
    if (c == u"get_scale") return SetReturn(timeScale_), true;
    if (c == u"cooldown")
    {
        // cooldown(str name, val seconds): true when at least that long has passed since it last said true
        std::string key = U(stage.scriptObject) + "#" + U(Arg(0));
        double secs = Arg(1).Num(), now = GetEngine().Now();
        auto it = cooldowns_.find(key);
        bool ready = it == cooldowns_.end() || now - it->second >= secs;
        if (ready) cooldowns_[key] = now;
        SetReturn(Bool(ready));
        return true;
    }
    return false;
}

/// Where a level (another project) is: Projects/<name> in the editor, data/levels/<name> in a built game.
std::string Glue3DSprite::LevelFolder(const std::string& name)
{
    if (BuildFlavor::IsEditor) return PathJoin(ProjectFiles::ProjectsRoot(), name);
    return PathJoin(PathJoin(PathJoin(ExeDir(), "data"), "levels"), name);
}

bool Glue3DSprite::LevelBlocks()
{
    String c = Cmd();
    if (c == u"load_scene")
    {
        std::string name = U(Arg(0));
        if (!ProjectFiles::IsProjectFolder(LevelFolder(name)))
        {
            TerminalPrint(T("error: there is no level (project) called \"" + name + "\""), u""_s);
            return true;
        }
        pendingLevel_ = name;
        stage.scene.Replace(stage.objectIndex.Num() + 35, 1); // wait here; the level changes before the next frame
        return true;
    }
    if (c == u"restart")
    {
        pendingRestart_ = true;
        stage.scene.Replace(stage.objectIndex.Num() + 35, 1);
        return true;
    }
    if (c == u"pause")
    {
        // pause(str true): every other object's script stops (UI elements keep running) until pause(str false)
        paused_ = !Eq(Arg(0), u"false"_s);
        pauseExempt_.clear();
        if (paused_) pauseExempt_.insert(U(stage.scriptObject));
        return true;
    }
    if (c == u"is_paused") return SetReturn(Bool(paused_)), true;
    if (c == u"current_scene") return SetReturn(T(currentLevel_)), true;
    return false;
}

bool Glue3DSprite::SaveGet(const std::string& key, Value& out)
{
    auto& data = SaveData(SaveFilePath());
    auto it = data.find(key);
    if (it == data.end()) return false;
    if (it->is_number()) out = it->get<double>();
    else if (it->is_string()) out = T(it->get<std::string>());
    else out = T(it->dump());
    return true;
}

void Glue3DSprite::SaveSet(const std::string& key, const Value& v)
{
    auto& data = SaveData(SaveFilePath());
    double d = v.Num();
    if (v.IsNumber() || (!U(v).empty() && U(Value(d)) == U(v)))
    {
        if (d == std::floor(d) && std::fabs(d) < 1e15) data[key] = (long long)d;
        else data[key] = d;
    }
    else
        data[key] = U(v);
    WriteSaveFile();
}

/// Every stage variable and list, to put the editor's project back after playing other levels.
struct Glue3DSprite::StageSnapshot
{
    std::vector<Value> Values;
    std::vector<std::vector<Value>> Lists;
};

void Glue3DSprite::LevelUpdate()
{
    ResetIfNewRun();
    // In the editor, stopping the game after load_scene brings back the project you were editing.
    if (levelBackup_ && !Eq(stage.allRun, 1))
    {
        RestoreSnapshot(*levelBackup_);
        levelBackup_.reset();
        currentLevel_.clear();
        if (Eq(stage.allRun, 1)) Glue3dStop(); // the snapshot was taken while the game ran
        stage.loadedModels.DeleteAll();
        stage.loadedModels.Add(0);
        EngineRefreshRenderQueue();
    }
    if (pendingRestart_)
    {
        pendingRestart_ = false;
        if (Eq(stage.allRun, 1))
        {
            Glue3dStop();
            Glue3dRun();
            EngineRefreshRenderQueue();
        }
    }
    if (!pendingLevel_.empty())
    {
        std::string name = pendingLevel_;
        pendingLevel_.clear();
        LoadLevel(name);
    }
}

void Glue3DSprite::LoadLevel(const std::string& name)
{
    bool editor = BuildFlavor::IsEditor && Eq(stage.allExportState, 0);
    if (editor && !levelBackup_) levelBackup_ = std::make_shared<StageSnapshot>(TakeSnapshot());
    Value fullscreen = stage.engineFullscreen, exportState = stage.allExportState;
    try
    {
        ProjectFiles::Load(LevelFolder(name), stage);
    }
    catch (const std::exception& e)
    {
        TerminalPrint(T(std::string("error: could not load level: ") + e.what()), u""_s);
        return;
    }
    // Start the level like a built game starts: no splash, no intro, straight into the game.
    stage.allExportState = exportState;
    stage.guiSplash = 0;
    stage.guiFreeze = 0;
    stage.errorPopup = 0;
    stage.allRun = 0;
    stage.allSplashscreen = 7;
    stage.ramEngineData.Replace(212, 0);
    StartEngine(0); // a built game (allExportState 1) starts running here
    if (!Eq(stage.allRun, 1))
    {
        Glue3dRun();
        EngineRefreshRenderQueue();
    }
    stage.engineFullscreen = fullscreen;
    currentLevel_ = name;
}

Glue3DSprite::StageSnapshot Glue3DSprite::TakeSnapshot()
{
    StageSnapshot snap;
    for (auto& [n, member] : Stage::ValueFields()) snap.Values.push_back(stage.*member);
    for (auto& [n, member] : Stage::ListFields()) snap.Lists.push_back((stage.*member).Items);
    return snap;
}

void Glue3DSprite::RestoreSnapshot(const StageSnapshot& snap)
{
    size_t i = 0;
    for (auto& [n, member] : Stage::ValueFields()) stage.*member = snap.Values[i++];
    i = 0;
    for (auto& [n, member] : Stage::ListFields())
    {
        (stage.*member).Items = snap.Lists[i++];
        (stage.*member).Version++;
    }
}

bool Glue3DSprite::LevelActive() const { return levelBackup_ != nullptr; }

// ------------------------------------------------------------------------------------------------
// Clones: clone (records parent / last clone), clone_with, parent, last_clone
// ------------------------------------------------------------------------------------------------

bool Glue3DSprite::CloneBlocks()
{
    String c = Cmd();
    if (c == u"clone" || c == u"clone_with")
    {
        Value me = stage.scriptObject;
        Value varName = Arg(1), varValue = Arg(2);
        Value tag = ReadData(me, u"tag"_s);
        // Glue3D's own clone (it reads the script name from the line, like object.clone(str script))
        stage.decodedElements.Replace(2, u"clone"_s);
        ProccessObject();
        Value clone = stage.objectName;
        if (!ObjectExists(clone)) return true;
        WriteData(clone, u"parent"_s, me);
        if (!Eq(tag, u"null"_s)) WriteData(clone, u"tag"_s, tag); // a clone keeps its parent's tag
        if (c == u"clone_with" && !Eq(varName, u""_s)) WriteData(clone, varName, varValue);
        lastClone_[U(me)] = U(clone);
        return true;
    }
    if (c == u"parent")
    {
        Value p = ReadData(stage.scriptObject, u"parent"_s);
        SetReturn(Eq(p, u"null"_s) ? u""_s : p);
        return true;
    }
    if (c == u"last_clone")
    {
        auto it = lastClone_.find(U(stage.scriptObject));
        SetReturn(it == lastClone_.end() ? u""_s : T(it->second));
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------------------------------
// Inline answers:  var d = object.distance(str player)   (any block that answers in data_return)
// ------------------------------------------------------------------------------------------------

/// Runs one block line (like a line of the script, but it doesn't move the script along).
void Glue3DSprite::RunBlockLine(const std::string& text)
{
    Value savedLine = stage.scriptLine;
    stage.scriptLine = T(text);
    DecodeIDSeperatorIncludeSeperatorSpecialdot(T(text + " "), u""_s, u" (),"_s, u""_s, u"."_s);
    if (!CustomCommand())
    {
        String kind = stage.decodedElements.Item(1).Str();
        if (kind == u"object") ProccessObject();
        else if (kind == u"model") ProccessModel();
        else if (kind == u"math") ProccessMath();
        else if (kind == u"physics") ProccessPhysics();
        else if (kind == u"game") ProccessGame();
        else if (kind == u"terminal") ProccessTerminal();
        else if (kind == u"sound") ProccessSound();
        else if (kind == u"interface") ProccessInterface();
        else if (kind == u"file") ProccessFile();
        else if (kind == u"time") ProccessTime();
        else if (kind == u"list") ProccessList();
        else TerminalPrint(T("error: \"" + text + "\" doesn't give an answer"), u""_s);
    }
    stage.scriptLine = savedLine;
}

bool Glue3DSprite::InlineAnswer()
{
    static const std::regex form(R"(^\s*var\s+(\S+)\s+(=|\+=|-=|\*=|/=|%=|\^=)\s+([A-Za-z_]+\.[A-Za-z0-9_]+\s*\(.*)$)");
    std::string line = U(stage.scriptLine);
    std::smatch m;
    if (!std::regex_match(line, m, form)) return false;
    std::string name = m[1], op = m[2], block = m[3];
    RunBlockLine(block);
    // a block that waits (terminal.input) answers later: this line runs again then
    if (!Lt(stage.scene.Item(stage.objectIndex.Num() + 35), 0.0125)) return true;
    Value v = ReadData(stage.scriptObject, u"data_return"_s);
    Value key = T(name);
    if (op == "=")
    {
        WriteData(stage.scriptObject, key, v);
        return true;
    }
    Value cur = ReadData(stage.scriptObject, key);
    double a = Eq(cur, u"null"_s) ? 0 : cur.Num(), b = v.Num(), r = a;
    if (op == "+=") r = a + b;
    else if (op == "-=") r = a - b;
    else if (op == "*=") r = a * b;
    else if (op == "/=") r = Nz(a / b);
    else if (op == "%=") r = Mod(a, b);
    else if (op == "^=") r = std::pow(a, b);
    WriteData(stage.scriptObject, key, r);
    return true;
}

// ------------------------------------------------------------------------------------------------
// Compound conditions:  if (var hp < val 1 and var lives > val 0)
// ------------------------------------------------------------------------------------------------

/// A value in a condition: (type, value).
Value Glue3DSprite::ConditionValue(const std::string& type, const std::string& value)
{
    if (type == "var") return ReadData(stage.scriptObject, T(value));
    if (type == "data_return") return ReadData(stage.scriptObject, u"data_return"_s);
    return T(value);
}

static bool Compare(const Value& a, const std::string& op, const Value& b, bool& ok)
{
    ok = true;
    if (op == "=" || op == "equals") return Eq(a, b);
    if (op == "!=" || op == "not_equals") return !Eq(a, b);
    if (op == ">" || op == "greater_than") return Gt(a, b);
    if (op == "<" || op == "less_than") return Lt(a, b);
    if (op == ">=" || op == "greater_equal") return !Lt(a, b);
    if (op == "<=" || op == "less_equal") return !Gt(a, b);
    if (op == "contains") return a.Str().Lower().view().find(b.Str().Lower().view()) != std::u16string_view::npos;
    ok = false;
    return false;
}

/// Glue3D's if / while / wait_until pass their whole condition as words; more than one comparison joined
/// with and / or (and "not" in front of one) is worked out here. "and" goes before "or", as usual.
bool Glue3DSprite::CompoundCondition(bool& result)
{
    std::vector<std::string> w;
    for (int i = 1; i <= stage.decodedElements.Length(); i++)
    {
        std::string t = U(stage.decodedElements.Item(i));
        if (!t.empty()) w.push_back(t);
    }
    if (w.size() <= 5) return false; // one comparison: Glue3D's own
    return EvalCondition(w, result);
}

/// A condition as words (type value op type value, joined with and / or, "not" in front); false if it can't be read.
bool Glue3DSprite::EvalCondition(const std::vector<std::string>& w, bool& result)
{
    bool any = false, all = true;
    size_t i = 0;
    while (i < w.size())
    {
        bool negate = false;
        while (i < w.size() && w[i] == "not")
        {
            negate = !negate;
            i++;
        }
        if (i + 1 >= w.size()) return false;
        bool value;
        Value a = ConditionValue(w[i], w[i + 1]);
        i += 2;
        bool ok = false;
        if (i < w.size() && w[i] != "and" && w[i] != "or")
        {
            if (i + 2 >= w.size()) return false; // op, type, value
            value = Compare(a, w[i], ConditionValue(w[i + 1], w[i + 2]), ok);
            if (!ok) return false;
            i += 3;
        }
        else
            value = Eq(a, u"true"_s); // just "var on_floor": is it true?
        if (negate) value = !value;
        all = all && value;
        // end of an "and" group?
        if (i >= w.size() || w[i] == "or")
        {
            any = any || all;
            all = true;
        }
        else if (w[i] != "and")
            return false;
        if (i < w.size()) i++; // skip and / or
    }
    result = any;
    return true;
}

// ------------------------------------------------------------------------------------------------
// Text helpers and chance (math category)
// ------------------------------------------------------------------------------------------------

/// Separators can't be typed as themselves in a script: "comma", "space" and "_" stand for them.
static std::string Separator(std::string s)
{
    if (s == "comma") return ",";
    if (s == "space" || s == "_") return " ";
    if (s == "dot") return ".";
    if (s == "colon") return ":";
    return s;
}

bool Glue3DSprite::TextBlocks()
{
    String c = Cmd();
    if (c == u"split")
    {
        // split(str text, str sep, val n): piece n (from 1); n = 0 gives how many pieces
        std::string text = U(Arg(0)), sep = Separator(U(Arg(1)));
        int n = (int)Arg(2).Num();
        std::vector<std::string> parts;
        if (sep.empty()) for (char ch : text) parts.push_back(std::string(1, ch));
        else
        {
            size_t a = 0, b;
            while ((b = text.find(sep, a)) != std::string::npos)
            {
                parts.push_back(text.substr(a, b - a));
                a = b + sep.size();
            }
            parts.push_back(text.substr(a));
            if (!parts.empty() && parts.back().empty() && sep == ",") parts.pop_back(); // "a,b," lists
        }
        if (n == 0) SetReturn((double)parts.size());
        else SetReturn(n >= 1 && n <= (int)parts.size() ? T(parts[n - 1]) : u""_s);
        return true;
    }
    if (c == u"substring")
    {
        // substring(str text, val start, val length): letters from start (1 = first)
        String text = Arg(0).Str();
        int start = std::max(1, (int)Arg(1).Num()), len = (int)Arg(2).Num();
        if (start > text.size() || len <= 0) return SetReturn(u""_s), true;
        len = std::min(len, text.size() - start + 1);
        SetReturn(text.Substr(start - 1, len));
        return true;
    }
    if (c == u"replace")
    {
        std::string text = U(Arg(0)), find = Separator(U(Arg(1))), with = U(Arg(2));
        if (with == "space" || with == "_") with = " ";
        else if (with == "nothing") with = "";
        if (!find.empty())
            for (size_t at = 0; (at = text.find(find, at)) != std::string::npos; at += with.size()) text.replace(at, find.size(), with);
        SetReturn(T(text));
        return true;
    }
    if (c == u"to_number")
    {
        double d = Arg(0).Num();
        SetReturn(std::isnan(d) ? 0 : d);
        return true;
    }
    if (c == u"chance")
    {
        static std::mt19937 rng{std::random_device{}()};
        double pct = Arg(0).Num();
        SetReturn(Bool(std::uniform_real_distribution<double>(0, 100)(rng) < pct));
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------------------------------
// Camera shake
// ------------------------------------------------------------------------------------------------

bool Glue3DSprite::PolishGameBlocks()
{
    String c = Cmd();
    if (c == u"camera_shake")
    {
        // camera_shake(val strength, val seconds): fades out over the time
        shakeStrength_ = std::max(0.0, Arg(0).Num());
        shakeLength_ = std::max(0.01, Eq(ArgType(1), u""_s) ? 0.5 : Arg(1).Num());
        shakeStart_ = GetEngine().Now();
        return true;
    }
    return false;
}

void Glue3DSprite::ShakeUpdate()
{
    static std::mt19937 rng{12345};
    double left = shakeStrength_ > 0 ? 1 - (GetEngine().Now() - shakeStart_) / shakeLength_ : 0;
    if (left <= 0 || !Eq(stage.allRun, 1))
    {
        shakeOffset_[0] = shakeOffset_[1] = shakeOffset_[2] = 0;
        if (left <= 0) shakeStrength_ = 0;
        return;
    }
    std::uniform_real_distribution<double> d(-1, 1);
    for (double& o : shakeOffset_) o = d(rng) * shakeStrength_ * 0.1 * left;
}

double Glue3DSprite::CameraShake(int axis) { return shakeOffset_[axis]; }

// ------------------------------------------------------------------------------------------------
// Gamepads (the first one connected)
// ------------------------------------------------------------------------------------------------

namespace {

int GamepadButton(const std::string& n)
{
    static const std::map<std::string, int> b = {
        {"a", GLFW_GAMEPAD_BUTTON_A}, {"b", GLFW_GAMEPAD_BUTTON_B}, {"x", GLFW_GAMEPAD_BUTTON_X}, {"y", GLFW_GAMEPAD_BUTTON_Y},
        {"lb", GLFW_GAMEPAD_BUTTON_LEFT_BUMPER}, {"rb", GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER}, {"back", GLFW_GAMEPAD_BUTTON_BACK},
        {"start", GLFW_GAMEPAD_BUTTON_START}, {"guide", GLFW_GAMEPAD_BUTTON_GUIDE}, {"ls", GLFW_GAMEPAD_BUTTON_LEFT_THUMB},
        {"rs", GLFW_GAMEPAD_BUTTON_RIGHT_THUMB}, {"up", GLFW_GAMEPAD_BUTTON_DPAD_UP}, {"down", GLFW_GAMEPAD_BUTTON_DPAD_DOWN},
        {"left", GLFW_GAMEPAD_BUTTON_DPAD_LEFT}, {"right", GLFW_GAMEPAD_BUTTON_DPAD_RIGHT}};
    auto it = b.find(n);
    return it == b.end() ? -1 : it->second;
}

int GamepadAxis(const std::string& n)
{
    static const std::map<std::string, int> a = {{"left_x", GLFW_GAMEPAD_AXIS_LEFT_X}, {"left_y", GLFW_GAMEPAD_AXIS_LEFT_Y},
                                                 {"right_x", GLFW_GAMEPAD_AXIS_RIGHT_X}, {"right_y", GLFW_GAMEPAD_AXIS_RIGHT_Y},
                                                 {"lt", GLFW_GAMEPAD_AXIS_LEFT_TRIGGER}, {"rt", GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER}};
    auto it = a.find(n);
    return it == a.end() ? -1 : it->second;
}

/// The first connected gamepad's state; false if there is none.
bool PadState(GLFWgamepadstate& st)
{
    for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; j++)
        if (glfwJoystickIsGamepad(j) && glfwGetGamepadState(j, &st)) return true;
    return false;
}

} // namespace

bool Glue3DSprite::IsInputDown(const std::string& key)
{
    if (key == "mouse") return GetEngine().Input.MouseDown;
    if (key.rfind("pad:", 0) == 0)
    {
        GLFWgamepadstate st;
        int b = GamepadButton(key.substr(4));
        return b >= 0 && PadState(st) && st.buttons[b] == GLFW_PRESS;
    }
    return KeyPressed(T(key));
}

bool Glue3DSprite::GamepadBlocks()
{
    String c = Cmd();
    if (c == u"gamepad_connected")
    {
        GLFWgamepadstate st;
        return SetReturn(Bool(PadState(st))), true;
    }
    if (c == u"gamepad_button")
    {
        std::string name = U(Arg(0));
        if (GamepadButton(name) < 0) TerminalPrint(T("unknown gamepad button \"" + name + "\""), 25);
        return SetReturn(Bool(IsInputDown("pad:" + name))), true;
    }
    if (c == u"gamepad_pressed") return SetReturn(Bool(KeyEdge("pad:" + U(Arg(0)), false))), true;
    if (c == u"gamepad_axis")
    {
        // -1..1 (sticks: up and right are +), triggers 0..1; small stick wobble counts as 0
        std::string name = U(Arg(0));
        int a = GamepadAxis(name);
        GLFWgamepadstate st;
        double v = 0;
        if (a >= 0 && PadState(st))
        {
            v = st.axes[a];
            if (name == "left_y" || name == "right_y") v = -v;
            if (name == "lt" || name == "rt") v = (v + 1) / 2;
            else if (std::fabs(v) < 0.15) v = 0;
        }
        else if (a < 0) TerminalPrint(T("unknown gamepad axis \"" + name + "\""), 25);
        SetReturn(v);
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------------------------------
// 3D sound: sound.play_at(str boom, str object, val range)
// ------------------------------------------------------------------------------------------------

bool Glue3DSprite::SoundAtBlocks()
{
    if (Cmd() != u"play_at") return false;
    Value sound = Arg(0), where = Arg(1);
    double range = Eq(ArgType(2), u""_s) ? 30 : std::max(0.1, Arg(2).Num());
    if (!ObjectExists(where))
    {
        TerminalPrint(u"object \""_s + where.Str() + u"\" doesn't exist!"_s, 25);
        return true;
    }
    double dx = ReadData(where, u"x"_s).Num() - stage.ramEngineData.Item(112).Num();
    double dy = ReadData(where, u"y"_s).Num() - stage.ramEngineData.Item(114).Num();
    double dz = ReadData(where, u"z"_s).Num() - stage.ramEngineData.Item(116).Num();
    double d = std::sqrt(dx * dx + dy * dy + dz * dz);
    double volume = 100 * std::max(0.0, 1 - d / range);
    if (volume <= 0.5) return true; // too far away to hear
    // its own voice (like play_advanced), so the volume doesn't change other sounds
    PlaySoundSoundName(T("3d#" + std::to_string(++soundCounter_)), sound, volume, 0);
    return true;
}

// ------------------------------------------------------------------------------------------------
// UI elements: color, text size and brightness, bars
// ------------------------------------------------------------------------------------------------

/// A UI setting of the element Glue3D is drawing (its record starts at stage.engineObjectindex).
double Glue3DSprite::UiStyle(const char16_t* key, double fallback)
{
    int idx = (int)stage.engineObjectindex.Num();
    int len = (int)stage.scene.Item(idx).Num();
    std::u16string_view k(key);
    for (int i = idx + 2; i + 1 < idx + len; i += 2)
        if (stage.scene.Item(i).Str().view() == k) return stage.scene.Item(i + 1).Num();
    return fallback;
}

bool Glue3DSprite::UiBlocks()
{
    String c = Cmd();
    const Value& me = stage.scriptObject;
    if (c == u"color") return WriteData(me, u"ui-color"_s, Arg(0).Num()), true;
    if (c == u"font_size") return WriteData(me, u"ui-font"_s, Arg(0).Num()), true;
    if (c == u"text_brightness") return WriteData(me, u"ui-text-brightness"_s, Arg(0).Num()), true;
    if (c == u"bar")
    {
        WriteData(me, u"ui-bar"_s, Arg(0).Num());
        WriteData(me, u"ui-bar-max"_s, Eq(ArgType(1), u""_s) ? 100 : Arg(1).Num());
        return true;
    }
    if (c == u"hide_bar") return WriteData(me, u"ui-bar-max"_s, 0), true;
    return false;
}

// ------------------------------------------------------------------------------------------------
// Keyframe animation on bones: model.keyframe / play_animation / stop_animation
// ------------------------------------------------------------------------------------------------

bool Glue3DSprite::AnimationBlocks()
{
    String c = Cmd();
    if (c == u"keyframe")
    {
        // keyframe(str walk, val time, str bone, val rx, val ry, val rz)
        std::string anim = U(Arg(0)), bone = U(Arg(2));
        double t = std::max(0.0, Arg(1).Num());
        auto& keys = animations_[anim][bone];
        Key k{t, {Arg(3).Num(), Arg(4).Num(), Arg(5).Num()}};
        auto it = std::find_if(keys.begin(), keys.end(), [&](const Key& o) { return std::fabs(o.Time - t) < 1e-9; });
        if (it != keys.end()) *it = k;
        else keys.push_back(k);
        std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) { return a.Time < b.Time; });
        return true;
    }
    if (c == u"play_animation")
    {
        std::string anim = U(Arg(0));
        if (!animations_.count(anim))
        {
            TerminalPrint(T("no animation \"" + anim + "\" (make it with model.keyframe)"), 25);
            return true;
        }
        auto& p = playing_[U(stage.scriptObject)];
        if (p.Name != anim || !p.Running) p.Start = GetEngine().Now();
        p.Name = anim;
        p.Loop = !Eq(Arg(1), u"once"_s);
        p.Running = true;
        p.Speed = Eq(ArgType(2), u""_s) ? 1 : Arg(2).Num();
        return true;
    }
    if (c == u"stop_animation")
    {
        auto it = playing_.find(U(stage.scriptObject));
        if (it != playing_.end()) it->second.Running = false;
        return true;
    }
    if (c == u"is_animating")
    {
        auto it = playing_.find(U(stage.scriptObject));
        return SetReturn(Bool(it != playing_.end() && it->second.Running)), true;
    }
    if (c == u"bone_angle")
    {
        // bone_angle(str bone, str x): the angle the animation has given that bone
        auto it = playing_.find(U(stage.scriptObject));
        std::string axis = U(Arg(1));
        int a = axis == "y" ? 1 : axis == "z" ? 2 : 0;
        double v = 0;
        if (it != playing_.end())
        {
            auto b = it->second.Applied.find(U(Arg(0)));
            if (b != it->second.Applied.end()) v = b->second[a];
        }
        return SetReturn(v), true;
    }
    return false;
}

/// Each frame: every playing animation turns its bones to where the keyframes say (with model.turn_bone).
void Glue3DSprite::AnimationUpdate()
{
    if (playing_.empty()) return;
    Value savedObject = stage.scriptObject, savedLine = stage.scriptLine;
    double now = GetEngine().Now();
    for (auto it = playing_.begin(); it != playing_.end();)
    {
        auto& [object, p] = *it;
        if (!ObjectExists(T(object)))
        {
            it = playing_.erase(it);
            continue;
        }
        auto anim = animations_.find(p.Name);
        if (!p.Running || anim == animations_.end())
        {
            ++it;
            continue;
        }
        double length = 0;
        for (auto& [bone, keys] : anim->second)
            if (!keys.empty()) length = std::max(length, keys.back().Time);
        double t = (now - p.Start) * p.Speed * TimeScale();
        if (length > 0 && p.Loop) t = std::fmod(t, length);
        else if (t >= length)
        {
            t = length;
            p.Running = false; // a "once" animation stops on its last keyframe
        }
        stage.scriptObject = T(object);
        for (auto& [bone, keys] : anim->second)
        {
            if (keys.empty()) continue;
            std::array<double, 3> target = keys.front().Rot;
            if (t >= keys.back().Time) target = keys.back().Rot;
            else
                for (size_t i = 1; i < keys.size(); i++)
                    if (t < keys[i].Time)
                    {
                        const Key &a = keys[i - 1], &b = keys[i];
                        double f = t <= a.Time ? 0 : (t - a.Time) / std::max(1e-9, b.Time - a.Time);
                        for (int j = 0; j < 3; j++) target[j] = a.Rot[j] + (b.Rot[j] - a.Rot[j]) * f;
                        break;
                    }
            auto& applied = p.Applied[bone];
            double d[3] = {target[0] - applied[0], target[1] - applied[1], target[2] - applied[2]};
            if (std::fabs(d[0]) + std::fabs(d[1]) + std::fabs(d[2]) < 1e-9) continue;
            std::ostringstream line;
            line.precision(10);
            line << "model.turn_bone(val " << d[0] << ", val " << d[1] << ", val " << d[2] << ", str " << bone << ")";
            RunBlockLine(line.str());
            applied = target;
        }
        ++it;
    }
    stage.scriptObject = savedObject;
    stage.scriptLine = savedLine;
}

// ------------------------------------------------------------------------------------------------
// Debug drawing: debug.show_hitboxes, debug.draw_line, debug.clear
// ------------------------------------------------------------------------------------------------

bool Glue3DSprite::DebugBlocks()
{
    String c = Cmd();
    if (c == u"show_hitboxes") return showHitboxes_ = !Eq(Arg(0), u"false"_s), true;
    if (c == u"draw_line")
    {
        // draw_line(val x1, val y1, val z1, val x2, val y2, val z2, val seconds)
        DebugLine l;
        for (int i = 0; i < 6; i++) l.P[i] = Arg(i).Num();
        l.Until = GetEngine().Now() + (Eq(ArgType(6), u""_s) ? 0.05 : Arg(6).Num());
        debugLines_.push_back(l);
        return true;
    }
    if (c == u"clear") return debugLines_.clear(), true;
    if (c == u"log")
    {
        // log(var name): prints "object: name = value" to the terminal
        TerminalPrint(T(U(stage.scriptObject) + ": " + U(stage.decodedElements.Item(4)) + " = " + U(Arg(0))), 0);
        return true;
    }
    return false;
}

/// A world point on the stage (stage units), using the game camera like Glue3D's renderer.
bool Glue3DSprite::WorldToStage(double x, double y, double z, Vec2& out)
{
    double yaw = stage.ramEngineData.Item(118).Num();
    double pm = std::fabs(stage.enginePerspectivemultiplier.Num());
    bool stretched = Eq(stage.ramEngineData.Item(178), 0);
    double kx = stretched ? 1.15 : 1, ky = stretched ? 1.15 + 0.06 * pm : 1;
    double ox = x - stage.ramEngineData.Item(112).Num(), oy = y - stage.ramEngineData.Item(114).Num(), oz = z - stage.ramEngineData.Item(116).Num();
    double sn = Sin(yaw), cs = Cos(yaw), fov = stage.engineFov.Num(), pitch = stage.ramEngineData.Item(120).Num();
    double nx = cs * ox - sn * oz, nz = sn * ox + cs * oz;
    if (nz <= 0.05) return false;
    double w = stage.engineScreenBottomrightx.Num() - stage.engineScreenTopleftx.Num();
    double h = stage.engineScreenToplefty.Num() - stage.engineScreenBottomrighty.Num();
    out = {(float)(kx * (stage.engineScreenMidx.Num() + w * (nx * fov / nz) / 480)),
           (float)(ky * (stage.engineScreenMidy.Num() + h * (oy * fov / nz - 3 * pitch) / 360))};
    return true;
}

void Glue3DSprite::DrawSystemsOverlay(Rect r)
{
    float k = r.w / 480.0f;
    auto toWin = [&](Vec2 p) { return Vec2(StageToWindowX(r, p.x), StageToWindowY(r, p.y)); };
    Rect view{StageToWindowX(r, stage.engineScreenTopleftx.Num()), StageToWindowY(r, stage.engineScreenToplefty.Num()), 0, 0};
    view.w = StageToWindowX(r, stage.engineScreenBottomrightx.Num()) - view.x;
    view.h = StageToWindowY(r, stage.engineScreenBottomrighty.Num()) - view.y;
    if (view.w <= 0 || view.h <= 0) return;
    gfx::Scissor(view);
    auto line3d = [&](double a[3], double b[3], Color col) {
        Vec2 p, q;
        if (WorldToStage(a[0], a[1], a[2], p) && WorldToStage(b[0], b[1], b[2], q)) gfx::DrawLine(toWin(p), toWin(q), 1.5f * k, col);
    };
    // UI bars (health bars)
    if (Eq(stage.allRun, 1))
        for (auto& o : ObjectRecords())
        {
            if (!Eq(RecordValue(o, u"model"), u"elementui"_s)) continue;
            double max = RecordValue(o, u"ui-bar-max", 0).Num();
            if (max <= 0) continue;
            double f = std::clamp(RecordValue(o, u"ui-bar", 0).Num() / max, 0.0, 1.0);
            double sx = std::fabs(stage.engineScreenTopleftx.Num() - stage.engineScreenBottomrightx.Num()) / 480;
            double sy = std::fabs(stage.engineScreenToplefty.Num() - stage.engineScreenBottomrighty.Num()) / 360;
            double cx = sx * RecordValue(o, u"x").Num() + stage.engineScreenMidx.Num();
            double cy = sy * RecordValue(o, u"y").Num() + stage.engineScreenMidy.Num();
            double scale = std::max(0.1, RecordValue(o, u"sx", 1).Num());
            float w = (float)(60 * scale * sx) * k, h = (float)(7 * scale * sy) * k;
            float x = StageToWindowX(r, cx) - w / 2, y = StageToWindowY(r, cy) - h / 2;
            gfx::DrawRectRounded({x - k, y - k, w + 2 * k, h + 2 * k}, 0.5f, 6, {10, 10, 14, 200});
            Color fill{(uint8_t)(230 * (1 - f) + 40 * f), (uint8_t)(60 * (1 - f) + 200 * f), 60, 255};
            if (f > 0) gfx::DrawRectRounded({x, y, std::max(h, (float)(w * f)), h}, 0.5f, 6, fill);
        }
    // hitboxes
    if (showHitboxes_)
        for (auto& o : ObjectRecords())
        {
            if (!IsSolid(o)) continue;
            double c[3], s[3];
            Box(o, c, s);
            double v[8][3];
            for (int i = 0; i < 8; i++)
                for (int a = 0; a < 3; a++) v[i][a] = c[a] + ((i >> a) & 1 ? s[a] : -s[a]) / 2;
            for (int i = 0; i < 8; i++)
                for (int a = 0; a < 3; a++)
                    if (!((i >> a) & 1)) line3d(v[i], v[i | (1 << a)], {80, 255, 120, 230});
        }
    // debug lines
    double now = GetEngine().Now();
    debugLines_.erase(std::remove_if(debugLines_.begin(), debugLines_.end(), [&](const DebugLine& l) { return now > l.Until; }),
                      debugLines_.end());
    for (auto& l : debugLines_)
    {
        double a[3] = {l.P[0], l.P[1], l.P[2]}, b[3] = {l.P[3], l.P[4], l.P[5]};
        line3d(a, b, {255, 220, 60, 255});
    }
    gfx::NoScissor();
}

} // namespace Glue3D
