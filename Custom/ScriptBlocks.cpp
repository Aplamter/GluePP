// Hand-written: GluePP's extra script blocks (the editor and built games both run these).
//
// Every script line goes through CustomCommand() first (tools/glue3d_edits.py hooks it into
// Glue3D's RunLineMainObjectScript); a line it doesn't know falls through to Glue3D's own blocks.
// The script editor's palette lists these blocks too (LIST_INSERTS in tools/glue3d_edits.py), and
// README.md documents every block.
//
// How a line looks to the interpreter:  category.command(type value, type value, ...)
//   decodedElements: 1 = category, 2 = command, then each argument as (type, value) at 3/4, 6/7, 9/10...
//   type "var" means: the value is the name of one of the object's variables (read it).
// A block that answers something puts the answer in the object's "data_return" variable.
#include <algorithm>
#include <cmath>
#include <ctime>
#include <random>

#include <GLFW/glfw3.h>

#include "BuildFlavor.h"
#include "Engine.h"
#include "Glue3DSprite.g.h"

namespace Glue3D {

using namespace scratch;

namespace {

Value T(const std::string& s) { return Value(String::FromUtf8(s)); }
std::string U(const Value& v) { return v.Str().ToUtf8(); }
Value Bool(bool b) { return b ? u"true"_s : u"false"_s; }

/// The first word of a script line ("if", "endrepeat", "object", ...): up to a space, "(" or ".".
std::string FirstWord(const Value& line)
{
    std::string s = U(line);
    size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return "";
    size_t b = s.find_first_of(" (.\t", a);
    return s.substr(a, b == std::string::npos ? std::string::npos : b - a);
}

std::mt19937& Rng()
{
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}

/// "a,b,c," -> {a, b, c}
std::vector<std::string> SplitItems(const std::string& s)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : s)
        if (c == ',')
        {
            out.push_back(cur);
            cur.clear();
        }
        else
            cur += c;
    if (!cur.empty()) out.push_back(cur);
    return out;
}

/// Script strings can't hold spaces (a space separates values), so "_" shows as a space in messages.
std::string Spaced(std::string s)
{
    std::replace(s.begin(), s.end(), '_', ' ');
    return s;
}

} // namespace

// ------------------------------------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------------------------------------

String Glue3DSprite::Cmd() { return stage.decodedElements.Item(2).Str(); }

Value Glue3DSprite::ArgType(int n) { return stage.decodedElements.Item(3 + 3 * n); }

Value Glue3DSprite::Arg(int n)
{
    Value type = stage.decodedElements.Item(3 + 3 * n), value = stage.decodedElements.Item(4 + 3 * n);
    if (Eq(type, u"var"_s) && U(value).find('[') != std::string::npos) return ReadVar(U(value)); // var slot[2]
    if (Eq(type, u"var"_s))
    {
        Value saved = stage.dataReturn;
        EditDataInObjectDataOperation(stage.scriptObject, value, 0, u"read"_s);
        Value v = stage.dataReturn;
        stage.dataReturn = saved;
        return v;
    }
    if (Eq(type, u"data_return"_s)) return ReadData(stage.scriptObject, u"data_return"_s);
    if (Eq(type, u"global"_s)) return ReadGlobal(U(value));
    if (Eq(type, u"local"_s)) return ReadVar(U(value));
    return value;
}

Value Glue3DSprite::ReadData(const Value& object, const Value& key)
{
    EditDataInObjectDataOperation(object, key, 0, u"read"_s);
    return stage.dataReturn;
}

void Glue3DSprite::WriteData(const Value& object, const Value& key, Value v)
{
    // Look in the object's own record (a variable holding the word "null" still counts as existing).
    int idx = (int)stage.scene.IndexOf(object.Str() + u".start"_s) - 1;
    if (idx < 1) return;
    int len = (int)stage.scene.Item(idx).Num();
    for (int i = idx + 2; i + 1 < idx + len; i += 2)
        if (Eq(stage.scene.Item(i), key))
        {
            stage.scene.Replace(i + 1, v);
            return;
        }
    InsertDataToObjectData(object, key, v);
}

void Glue3DSprite::SetReturn(Value v) { WriteData(stage.scriptObject, u"data_return"_s, v); }

bool Glue3DSprite::ObjectExists(const Value& name) { return !Eq(name, u""_s) && stage.scene.IndexOf(name.Str() + u".start"_s) > 0; }

bool Glue3DSprite::HasPhysics()
{
    if (!Eq(ReadData(stage.scriptObject, u"physics"_s), u"null"_s)) return true;
    TerminalPrint(u"physics not included! (use physics.include_physics() first)"_s, 25);
    return false;
}

/// The line to run next is `index` (an index into loadedScripts). Glue3D adds 1 to the program counter
/// after a line, and the line it runs is scriptHeader + 1 + counter.
void Glue3DSprite::JumpTo(int index)
{
    stage.scene.Replace(stage.objectIndex.Num() + 31, index - stage.scriptHeader.Num() - 2);
}

/// From a block's first line, the line that closes it (`close`), skipping nested blocks; 0 if none.
int Glue3DSprite::FindBlockEnd(int from, const char* open, const char* close)
{
    int end = (int)stage.loadedScripts.IndexOf(stage.scriptName.Str() + u".end"_s);
    int depth = 0;
    for (int i = from; i < end; i++)
    {
        std::string w = FirstWord(stage.loadedScripts.Item(i));
        if (w == open) depth++;
        else if (w == close && --depth == 0) return i;
    }
    return 0;
}

/// From a block's closing line, the line that opened it; 0 if none.
int Glue3DSprite::FindBlockStart(int from, const char* open, const char* close)
{
    int start = (int)stage.scriptHeader.Num();
    int depth = 0;
    for (int i = from; i > start; i--)
    {
        std::string w = FirstWord(stage.loadedScripts.Item(i));
        if (w == close) depth++;
        else if (w == open && --depth == 0) return i;
    }
    return 0;
}

std::string Glue3DSprite::LoopKey(int line) { return U(stage.scriptObject) + "@" + U(stage.scriptName) + "#" + std::to_string(line); }

/// A condition written like if's: (type value op type value), from the line's 3rd element on.
bool Glue3DSprite::Condition()
{
    Value text = u""_s;
    for (int i = 3; i <= stage.decodedElements.Length(); i++) text = text.Str() + stage.decodedElements.Item(i).Str() + u" "_s;
    DecodeIDSeperatorIncludeSeperatorSpecialdot(text, u""_s, u" "_s, u""_s, u""_s);
    Operation(stage.decodedElements.Item(1), stage.decodedElements.Item(2), stage.decodedElements.Item(3), stage.decodedElements.Item(4),
              stage.decodedElements.Item(5));
    return Eq(stage.dataReturn, u"true"_s);
}

bool Glue3DSprite::ListRead(const Value& name, std::vector<std::string>& items)
{
    FileFindData(name);
    if (!Eq(stage.fileFound, 1))
    {
        TerminalPrint(u"list \""_s + name.Str() + u"\" doesn't exist!"_s, 25);
        return false;
    }
    items = SplitItems(U(stage.engineFiles.Item(stage.fileLoadIndex)));
    if (!items.empty()) items.erase(items.begin()); // the list's name
    return true;
}

void Glue3DSprite::ListWrite(const Value& name, const std::vector<std::string>& items)
{
    FileFindData(name);
    if (!Eq(stage.fileFound, 1)) return;
    std::string text = U(name) + ",";
    for (auto& s : items) text += s + ",";
    DeleteNameIfDataObject(u"data"_s, stage.fileLoadIndex.Num() - 1, stage.fileFolderIndex, u""_s);
    InsertDataToFile(stage.engineFiles.Item(stage.fileFolderIndex), u"list"_s, T(text));
}

// ------------------------------------------------------------------------------------------------
// Extra operators for if / while / wait_until (Glue3D's Operation calls this first)
// ------------------------------------------------------------------------------------------------

bool Glue3DSprite::CustomOperation(const Value& op)
{
    // if / while / wait_until: the condition is read from the script line itself, so it can use math, brackets,
    // global variables, arrays and blocks that answer (Custom/ScriptVariables.cpp)
    if (Eq(stage.decodedElements.Item(3), op))
    {
        bool result;
        if (LineCondition(result) || CompoundCondition(result))
        {
            stage.dataReturn = Bool(result);
            return true;
        }
    }
    const Value &a = stage.dataVal1, &b = stage.dataVal2;
    std::u16string_view o = op.Str().view();
    auto answer = [&](bool r) {
        stage.dataReturn = Bool(r);
        return true;
    };
    if (o == u"!=" || o == u"not_equals") return answer(!Eq(a, b));
    if (o == u">=" || o == u"greater_equal") return answer(!Lt(a, b));
    if (o == u"<=" || o == u"less_equal") return answer(!Gt(a, b));
    if (o == u"contains") return answer(a.Str().Lower().view().find(b.Str().Lower().view()) != std::u16string_view::npos);
    return false;
}

// ------------------------------------------------------------------------------------------------
// The blocks
// ------------------------------------------------------------------------------------------------

bool Glue3DSprite::CustomCommand()
{
    if (SystemsCommand()) return true; // Custom/ScriptSystems.cpp
    String kind = stage.decodedElements.Item(1).Str();
    if (kind == u"object") return ObjectBlocks();
    if (kind == u"physics") return PhysicsBlocks();
    if (kind == u"game") return GameBlocks();
    if (kind == u"math") return MathBlocks();
    if (kind == u"var") return VariableBlocks();
    if (kind == u"terminal") return TerminalBlocks();
    if (kind == u"interface") return InterfaceBlocks();
    if (kind == u"time") return TimeBlocks();
    if (kind == u"list") return ListBlocks();
    if (kind == u"sound") return SoundBlocks();
    if (kind == u"model") return ModelBlocks();
    return FlowBlocks();
}

// ---- other: else, repeat, for, wait_until ----
bool Glue3DSprite::FlowBlocks()
{
    std::string w = U(stage.decodedElements.Item(1));
    int line = (int)stage.scriptReadindex.Num();

    if (w == "else")
    {
        // Reached at the end of the if's "true" part: carry on after endif. (When the condition was false,
        // Glue3D's if jumps straight past the else.)
        int endif = 0, end = (int)stage.loadedScripts.IndexOf(stage.scriptName.Str() + u".end"_s), depth = 1;
        for (int i = line + 1; i < end && !endif; i++)
        {
            std::string x = FirstWord(stage.loadedScripts.Item(i));
            if (x == "if") depth++;
            else if (x == "endif" && --depth == 0) endif = i;
        }
        if (endif) JumpTo(endif + 1);
        else TerminalPrint(u"error: else without endif!"_s, u""_s);
        return true;
    }
    if (w == "repeat")
    {
        int times = (int)std::floor(Arg(0).Num());
        int end = FindBlockEnd(line, "repeat", "endrepeat");
        if (!end)
        {
            TerminalPrint(u"error: repeat without endrepeat!"_s, u""_s);
            return true;
        }
        if (times <= 0)
        {
            loopCounters_.erase(LoopKey(line));
            JumpTo(end + 1);
        }
        else
            loopCounters_[LoopKey(line)] = {(double)times, 0, 0};
        return true;
    }
    if (w == "endrepeat")
    {
        int start = FindBlockStart(line, "repeat", "endrepeat");
        auto it = start ? loopCounters_.find(LoopKey(start)) : loopCounters_.end();
        if (it == loopCounters_.end()) return true;
        if (--it->second.Count > 0)
            JumpTo(start + 1);
        else
            loopCounters_.erase(it);
        return true;
    }
    if (w == "for")
    {
        // for (var i, val from, val to, val step)
        Value name = stage.decodedElements.Item(4);
        double from = Arg(1).Num(), to = Arg(2).Num();
        double step = Eq(ArgType(3), u""_s) ? (to >= from ? 1 : -1) : Arg(3).Num();
        int end = FindBlockEnd(line, "for", "endfor");
        if (!end)
        {
            TerminalPrint(u"error: for without endfor!"_s, u""_s);
            return true;
        }
        WriteData(stage.scriptObject, name, from);
        if (step == 0 || (step > 0 ? from > to : from < to))
        {
            loopCounters_.erase(LoopKey(line));
            JumpTo(end + 1);
        }
        else
            loopCounters_[LoopKey(line)] = {0, to, step};
        return true;
    }
    if (w == "endfor")
    {
        int start = FindBlockStart(line, "for", "endfor");
        auto it = start ? loopCounters_.find(LoopKey(start)) : loopCounters_.end();
        if (it == loopCounters_.end()) return true;
        DecodeIDSeperatorIncludeSeperatorSpecialdot(stage.loadedScripts.Item(start).Str() + u" "_s, u""_s, u" (),"_s, u""_s, u"."_s);
        Value name = stage.decodedElements.Item(4);
        double v = ReadData(stage.scriptObject, name).Num() + it->second.Step;
        WriteData(stage.scriptObject, name, v);
        if (it->second.Step > 0 ? v <= it->second.To : v >= it->second.To)
            JumpTo(start + 1);
        else
            loopCounters_.erase(it);
        return true;
    }
    if (w == "wait_until")
    {
        // The program counter only moves on while the object's wait value (+35) is about 0.
        stage.scene.Replace(stage.objectIndex.Num() + 35, Condition() ? 0 : 1);
        return true;
    }
    if (w == "stop")
    {
        TerminalPrint(u"program stopped"_s, -35);
        stage.scene.Replace(stage.objectIndex.Num() + 31, u"halt"_s);
        return true;
    }
    return false;
}

// ---- object ----
bool Glue3DSprite::ObjectBlocks()
{
    String c = Cmd();
    const Value& me = stage.scriptObject;
    auto set3 = [&](const Value& a, const Value& b, const Value& d, bool add) {
        Value x = Arg(0), y = Arg(1), z = Arg(2), op = add ? u"add"_s : u"set"_s;
        EditDataInObjectDataOperation(me, a, x, op);
        EditDataInObjectDataOperation(me, b, y, op);
        EditDataInObjectDataOperation(me, d, z, op);
    };
    if (c == u"set_pos") return set3(u"x"_s, u"y"_s, u"z"_s, false), true;
    if (c == u"set_rotation") return set3(u"rx"_s, u"ry"_s, u"rz"_s, false), true;
    if (c == u"set_scale") return set3(u"sx"_s, u"sy"_s, u"sz"_s, false), true;
    if (c == u"change_scale") return set3(u"sx"_s, u"sy"_s, u"sz"_s, true), true;
    if (c == u"get_pos" || c == u"get_rotation" || c == u"get_scale")
    {
        std::string axis = U(Arg(0));
        if (axis != "x" && axis != "y" && axis != "z") axis = "x";
        std::string key = (c == u"get_pos" ? "" : c == u"get_rotation" ? "r" : "s") + axis;
        SetReturn(ReadData(me, T(key)));
        return true;
    }
    if (c == u"look_at")
    {
        Value other = Arg(0);
        if (!ObjectExists(other))
        {
            TerminalPrint(u"object \""_s + other.Str() + u"\" doesn't exist!"_s, 25);
            return true;
        }
        double tx = ReadData(other, u"x"_s).Num(), tz = ReadData(other, u"z"_s).Num();
        double x = ReadData(me, u"x"_s).Num(), z = ReadData(me, u"z"_s).Num();
        // the same angle rotate_toward turns to
        double yaw = -(Atan(Nz((tx - x) / (tz - z))) + 180 * (z > tz ? 1 : 0));
        EditDataInObjectDataOperation(me, u"rx"_s, yaw, u"set"_s);
        return true;
    }
    if (c == u"move_forward")
    {
        double dir = ReadData(me, u"rx"_s).Num(), d = Arg(0).Num();
        EditDataInObjectDataOperation(me, u"x"_s, Nz(Cos(dir + 90)) * d, u"add"_s);
        EditDataInObjectDataOperation(me, u"z"_s, Nz(Sin(dir + 90)) * d, u"add"_s);
        return true;
    }
    if (c == u"distance_to_point")
    {
        double dx = Arg(0).Num() - ReadData(me, u"x"_s).Num(), dy = Arg(1).Num() - ReadData(me, u"y"_s).Num(),
               dz = Arg(2).Num() - ReadData(me, u"z"_s).Num();
        SetReturn(std::sqrt(dx * dx + dy * dy + dz * dz));
        return true;
    }
    if (c == u"hide")
    {
        Value model = ReadData(me, u"model"_s);
        if (!Eq(model, u""_s)) WriteData(me, u"hidden_model"_s, model);
        EditDataInObjectDataOperation(me, u"model"_s, u""_s, u"set"_s);
        EngineRefreshRenderQueue();
        return true;
    }
    if (c == u"show")
    {
        Value model = ReadData(me, u"hidden_model"_s);
        if (!Eq(model, u"null"_s) && !Eq(model, u""_s))
        {
            EditDataInObjectDataOperation(me, u"model"_s, model, u"set"_s);
            EditDataInObjectDataOperation(me, u"hidden_model"_s, u""_s, u"set"_s);
            EngineRefreshRenderQueue();
        }
        return true;
    }
    if (c == u"is_visible") return SetReturn(Bool(!Eq(ReadData(me, u"model"_s), u""_s))), true;
    if (c == u"exists") return SetReturn(Bool(ObjectExists(Arg(0)))), true;
    if (c == u"my_name") return SetReturn(me), true;
    if (c == u"new_attribute")
    {
        // new_attribute(str object, str name, val value): give an object a new variable (or set it)
        Value other = Arg(0), name = Arg(1), value = Arg(2);
        if (!ObjectExists(other))
        {
            TerminalPrint(u"object \""_s + other.Str() + u"\" doesn't exist!"_s, 25);
            return true;
        }
        WriteData(other, name, value);
        return true;
    }
    return false;
}

// ---- physics ----
bool Glue3DSprite::PhysicsBlocks()
{
    String c = Cmd();
    const Value& me = stage.scriptObject;
    static const std::u16string_view needsPhysics[] = {u"set_velocity", u"get_velocity", u"stop", u"jump", u"set_gravity", u"set_friction",
                                                       u"set_bounce", u"set_jump", u"is_moving", u"speed"};
    if (std::find(std::begin(needsPhysics), std::end(needsPhysics), c.view()) == std::end(needsPhysics)) return false;
    if (!HasPhysics()) return true;
    if (c == u"set_velocity")
    {
        EditDataInObjectDataOperation(me, u"velx"_s, Arg(0), u"set"_s);
        EditDataInObjectDataOperation(me, u"vely"_s, Arg(1), u"set"_s);
        EditDataInObjectDataOperation(me, u"velz"_s, Arg(2), u"set"_s);
    }
    else if (c == u"get_velocity")
    {
        std::string axis = U(Arg(0));
        if (axis != "x" && axis != "y" && axis != "z") axis = "x";
        SetReturn(ReadData(me, T("vel" + axis)));
    }
    else if (c == u"stop")
    {
        for (auto k : {u"velx"_s, u"vely"_s, u"velz"_s}) EditDataInObjectDataOperation(me, k, 0, u"set"_s);
    }
    else if (c == u"jump")
    {
        // only from the floor, like the space bar in game.movement
        if (Eq(ReadData(me, u"stand"_s), 1))
        {
            EditDataInObjectDataOperation(me, u"stand"_s, 0, u"set"_s);
            Value h = Eq(ArgType(0), u""_s) ? ReadData(me, u"jumpheight"_s) : Arg(0);
            EditDataInObjectDataOperation(me, u"vely"_s, h, u"set"_s);
        }
    }
    else if (c == u"set_gravity") EditDataInObjectDataOperation(me, u"gravity"_s, Arg(0), u"set"_s);
    else if (c == u"set_friction") EditDataInObjectDataOperation(me, u"friction"_s, Arg(0), u"set"_s);
    else if (c == u"set_bounce") EditDataInObjectDataOperation(me, u"bounce"_s, Arg(0), u"set"_s);
    else if (c == u"set_jump") EditDataInObjectDataOperation(me, u"jumpheight"_s, Arg(0), u"set"_s);
    else if (c == u"is_moving" || c == u"speed")
    {
        double x = ReadData(me, u"velx"_s).Num(), y = ReadData(me, u"vely"_s).Num(), z = ReadData(me, u"velz"_s).Num();
        if (c == u"speed") SetReturn(std::sqrt(x * x + z * z));
        else SetReturn(Bool(std::sqrt(x * x + y * y + z * z) > 0.01));
    }
    return true;
}

// ---- game ----
bool Glue3DSprite::GameBlocks()
{
    String c = Cmd();
    auto& input = GetEngine().Input;
    if (c == u"mouse_down") return SetReturn(Bool(input.MouseDown)), true;
    if (c == u"right_mouse_down") return SetReturn(Bool(input.RightMouseDown)), true;
    if (c == u"any_key") return SetReturn(Bool(KeyPressed(u"any"_s))), true;
    if (c == u"mouse_motion")
    {
        SetReturn(Eq(Arg(0), u"y"_s) ? input.LookDeltaY : input.LookDeltaX);
        return true;
    }
    if (c == u"lock_mouse")
    {
        input.SetMouseCapture(!Eq(Arg(0), u"false"_s));
        return true;
    }
    if (c == u"mouse_locked") return SetReturn(Bool(input.MouseCaptured())), true;
    if (c == u"get_camera") return SetReturn(stage.engineGamecamera), true;
    if (c == u"message")
    {
        Value text = Arg(0);
        double seconds = Eq(ArgType(1), u""_s) ? 2 : Arg(1).Num();
        gameMessage_ = Spaced(U(text));
        gameMessageUntil_ = GetEngine().Now() + std::max(0.0, seconds);
        return true;
    }
    if (c == u"quit")
    {
        if constexpr (BuildFlavor::IsEditor)
        {
            // In the editor the game just stops (a built game closes its window).
            TerminalPrint(u"game.quit: the built game closes here"_s, -35);
            stage.scene.Replace(stage.objectIndex.Num() + 31, u"halt"_s);
        }
        else if (GetEngine().Window())
            glfwSetWindowShouldClose((GLFWwindow*)GetEngine().Window(), 1);
        return true;
    }
    return false;
}

/// game.message's text, drawn over the game (hooked through GameControlsDraw).
void Glue3DSprite::ScriptBlocksDraw(Rect stageRect)
{
    DrawSystemsOverlay(stageRect);  // UI bars, debug drawing (Custom/ScriptSystems.cpp)
    DrawTerminalOverlay(stageRect); // watched lines, terminal overlay, console (Custom/ScriptTerminal.cpp)
    if (gameMessage_.empty() || GetEngine().Now() > gameMessageUntil_) return;
    float k = stageRect.w / 480.0f, size = 14 * k;
    auto& mon = GetEngine().monitors;
    float w = mon.Measure(gameMessage_, size);
    float x = StageToWindowX(stageRect, (stage.engineScreenTopleftx.Num() + stage.engineScreenBottomrightx.Num()) / 2) - w / 2;
    float y = StageToWindowY(stageRect, stage.engineScreenToplefty.Num()) + 18 * k;
    gfx::DrawRectRounded({x - 8 * k, y - 4 * k, w + 16 * k, size + 8 * k}, 0.4f, 6, {15, 18, 26, 190});
    mon.Text(gameMessage_, x, y, size, Color::White());
}

// ---- math ----
bool Glue3DSprite::MathBlocks()
{
    String c = Cmd();
    auto n = [&](int i) { return Arg(i).Num(); };
    auto put = [&](double v) {
        SetReturn(v);
        return true;
    };
    if (c == u"min") return put(std::min(n(0), n(1)));
    if (c == u"max") return put(std::max(n(0), n(1)));
    if (c == u"round") return put(std::round(n(0)));
    if (c == u"ceil") return put(std::ceil(n(0)));
    if (c == u"sign") return put(n(0) > 0 ? 1 : n(0) < 0 ? -1 : 0);
    if (c == u"atan2") return put(std::atan2(n(0), n(1)) * 180 / kPi);
    if (c == u"pi") return put(kPi);
    if (c == u"exp") return put(std::exp(n(0)));
    if (c == u"ln") return put(std::log(n(0)));
    if (c == u"lerp") return put(n(0) + (n(1) - n(0)) * n(2));
    if (c == u"snap")
    {
        double step = n(1);
        return put(step == 0 ? n(0) : std::round(n(0) / step) * step);
    }
    if (c == u"random_int")
    {
        long a = (long)std::ceil(std::min(n(0), n(1))), b = (long)std::floor(std::max(n(0), n(1)));
        if (b < a) return put(a);
        return put((double)std::uniform_int_distribution<long>(a, b)(Rng()));
    }
    if (c == u"distance_2d") return put(std::hypot(n(2) - n(0), n(3) - n(1)));
    if (c == u"distance_3d")
    {
        double dx = n(3) - n(0), dy = n(4) - n(1), dz = n(5) - n(2);
        return put(std::sqrt(dx * dx + dy * dy + dz * dz));
    }
    if (c == u"not_equals" || c == u"greater_equal" || c == u"less_equal" || c == u"contains")
    {
        stage.dataVal1 = Arg(0);
        stage.dataVal2 = Arg(1);
        CustomOperation(c);
        SetReturn(stage.dataReturn);
        return true;
    }
    if (c == u"length") return put(Arg(0).Str().Length());
    if (c == u"upper") return SetReturn(Arg(0).Str().Upper()), true;
    if (c == u"lower") return SetReturn(Arg(0).Str().Lower()), true;
    return false;
}

// ---- variables: /=  %=  ^= ----
bool Glue3DSprite::VariableBlocks()
{
    String op = stage.decodedElements.Item(3).Str();
    if (op != u"/=" && op != u"%=" && op != u"^=") return false;
    Value name = stage.decodedElements.Item(2);
    Value type = stage.decodedElements.Item(4), raw = stage.decodedElements.Item(5);
    double b = Eq(type, u"var"_s) ? ReadData(stage.scriptObject, raw).Num()
             : Eq(type, u"data_return"_s) ? ReadData(stage.scriptObject, u"data_return"_s).Num()
                                          : raw.Num();
    Value cur = ReadData(stage.scriptObject, name);
    if (Eq(cur, u"null"_s))
    {
        TerminalPrint(u"variable \""_s + name.Str() + u"\" not found"_s, 0);
        return true;
    }
    double a = cur.Num(), r;
    if (op == u"/=") r = Nz(a / b);
    else if (op == u"%=") r = Mod(a, b);
    else r = std::pow(a, b);
    EditDataInObjectDataOperation(stage.scriptObject, name, r, u"set"_s);
    return true;
}

// ---- terminal ----
bool Glue3DSprite::TerminalBlocks()
{
    String c = Cmd();
    if (c == u"print_return")
    {
        TerminalPrint(ReadData(stage.scriptObject, u"data_return"_s), Arg(0).Num() + 15);
        return true;
    }
    if (c == u"print_value")
    {
        // print_value(str label, var name, val color): "label: value"
        TerminalPrint(T(Spaced(U(stage.decodedElements.Item(4))) + ": " + U(Arg(1))), Arg(2).Num() + 15);
        return true;
    }
    if (c == u"warn")
    {
        TerminalPrint(T("warning: " + Spaced(U(Arg(0)))), 25);
        return true;
    }
    return false;
}

// ---- interface ----
bool Glue3DSprite::InterfaceBlocks()
{
    String c = Cmd();
    const Value& me = stage.scriptObject;
    if (c == u"get_pos")
    {
        SetReturn(ReadData(me, Eq(Arg(0), u"y"_s) ? u"y"_s : u"x"_s));
        return true;
    }
    if (c == u"move_by")
    {
        EditDataInObjectDataOperation(me, u"x"_s, Arg(0), u"add"_s);
        EditDataInObjectDataOperation(me, u"y"_s, Arg(1), u"add"_s);
        return true;
    }
    if (c == u"change_scale")
    {
        EditDataInObjectDataOperation(me, u"sx"_s, Arg(0), u"add"_s);
        return true;
    }
    if (c == u"turn")
    {
        EditDataInObjectDataOperation(me, u"rx"_s, Arg(0), u"add"_s);
        return true;
    }
    if (c == u"hide" || c == u"show")
    {
        EditDataInObjectDataOperation(me, u"ui-ghost"_s, c == u"hide" ? 100 : 0, u"set"_s);
        return true;
    }
    if (c == u"get_text") return SetReturn(ReadData(me, u"ui-text"_s)), true;
    if (c == u"hovered")
    {
        // the mouse within `radius` of the element's position (stage units)
        double r = Eq(ArgType(0), u""_s) ? 20 : Arg(0).Num();
        double dx = MouseX() - ReadData(me, u"x"_s).Num(), dy = MouseY() - ReadData(me, u"y"_s).Num();
        SetReturn(Bool(dx * dx + dy * dy <= r * r));
        return true;
    }
    return false;
}

// ---- time ----
bool Glue3DSprite::TimeBlocks()
{
    String c = Cmd();
    if (c == u"fps")
    {
        double d = stage.timeDelta.Num();
        SetReturn(d > 0 ? std::round(1 / d) : 0);
        return true;
    }
    if (c == u"date")
    {
        std::time_t now = std::time(nullptr);
        std::tm t = *std::localtime(&now);
        std::string part = U(Arg(0));
        double v = part == "year" ? t.tm_year + 1900 : part == "month" ? t.tm_mon + 1 : part == "day" ? t.tm_mday
                 : part == "hour" ? t.tm_hour : part == "minute" ? t.tm_min : part == "second" ? t.tm_sec
                 : part == "weekday" ? (t.tm_wday == 0 ? 7 : t.tm_wday) : 0;
        SetReturn(v);
        return true;
    }
    if (c == u"seconds")
    {
        SetReturn((double)std::time(nullptr));
        return true;
    }
    return false;
}

// ---- lists ----
bool Glue3DSprite::ListBlocks()
{
    String c = Cmd();
    static const std::u16string_view mine[] = {u"contains", u"index_of", u"insert_item", u"random_item", u"last_item", u"remove_last", u"join"};
    if (std::find(std::begin(mine), std::end(mine), c.view()) == std::end(mine)) return false;
    // (read the values first: looking the list up decodes text and replaces the line's elements)
    Value name = Arg(0), arg1 = Arg(1), arg2 = Arg(2), type1 = ArgType(1);
    std::vector<std::string> items;
    if (!ListRead(name, items)) return true;
    if (c == u"contains" || c == u"index_of")
    {
        std::string want = U(arg1);
        auto it = std::find(items.begin(), items.end(), want);
        if (c == u"contains") SetReturn(Bool(it != items.end()));
        else SetReturn(it == items.end() ? 0 : (double)(it - items.begin() + 1));
    }
    else if (c == u"insert_item")
    {
        int at = std::clamp((int)arg1.Num(), 1, (int)items.size() + 1);
        items.insert(items.begin() + (at - 1), U(arg2));
        ListWrite(name, items);
    }
    else if (c == u"random_item")
        SetReturn(items.empty() ? u""_s : T(items[std::uniform_int_distribution<size_t>(0, items.size() - 1)(Rng())]));
    else if (c == u"last_item")
        SetReturn(items.empty() ? u""_s : T(items.back()));
    else if (c == u"remove_last")
    {
        if (!items.empty())
        {
            SetReturn(T(items.back()));
            items.pop_back();
            ListWrite(name, items);
        }
    }
    else if (c == u"join")
    {
        std::string sep = Eq(type1, u""_s) ? " " : Spaced(U(arg1)), out;
        for (size_t i = 0; i < items.size(); i++) out += (i ? sep : "") + items[i];
        SetReturn(T(out));
    }
    return true;
}

// ---- sound ----
bool Glue3DSprite::SoundBlocks()
{
    String c = Cmd();
    if (c == u"stop_all")
    {
        GetEngine().Audio.StopAll();
        return true;
    }
    if (c == u"volume")
    {
        SetVolume(Arg(0).Num());
        return true;
    }
    if (c == u"pitch")
    {
        SetSoundEffect(SoundEffect::Pitch, Arg(0).Num());
        return true;
    }
    return false;
}

// ---- model ----
bool Glue3DSprite::ModelBlocks()
{
    String c = Cmd();
    if (c == u"get_model")
    {
        Value m = ReadData(stage.scriptObject, u"model"_s);
        if (Eq(m, u""_s)) m = ReadData(stage.scriptObject, u"hidden_model"_s);
        SetReturn(Eq(m, u"null"_s) ? u""_s : m);
        return true;
    }
    if (c == u"copy_model")
    {
        // copy_model(str object): use the same model as another object
        Value other = Arg(0);
        if (!ObjectExists(other))
        {
            TerminalPrint(u"object \""_s + other.Str() + u"\" doesn't exist!"_s, 25);
            return true;
        }
        EditDataInObjectDataOperation(stage.scriptObject, u"model"_s, ReadData(other, u"model"_s), u"set"_s);
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------------------------------
// Script editor helpers
// ------------------------------------------------------------------------------------------------

bool Glue3DSprite::BlockIndents(const Value& w)
{
    return Eq(w, u"repeat "_s) || Eq(w, u"for "_s) || Eq(w, u"else "_s) || Eq(w, u"func "_s);
}

bool Glue3DSprite::BlockDedents(const Value& w)
{
    return Eq(w, u"endrepeat "_s) || Eq(w, u"endfor "_s) || Eq(w, u"else "_s) || Eq(w, u"endfunc "_s);
}

bool Glue3DSprite::BlockReturnsValue(const Value& category, const Value& command)
{
    static const std::map<std::string, std::vector<std::string>> reporters = {
        {"object", {"get_pos", "get_rotation", "get_scale", "distance_to_point", "is_visible", "exists", "my_name", "get_tag", "nearest", "count", "parent", "last_clone"}},
        {"physics", {"get_velocity", "is_moving", "speed", "touching", "touching_any", "raycast"}},
        {"game", {"mouse_down", "right_mouse_down", "any_key", "mouse_motion", "mouse_locked", "get_camera", "key_down", "key_up", "mouse_clicked", "current_scene", "is_paused", "gamepad_connected", "gamepad_button", "gamepad_pressed", "gamepad_axis"}},
        {"event", {"received"}},
        {"terminal", {"command", "command_arg", "input"}},
        {"var", {"length", "pop", "get", "exists", "names", "changed", "previous", "type", "is_number", "round_to"}},
        {"save", {"get", "has"}},
        {"model", {"get_model", "is_animating", "bone_angle"}},
        {"interface", {"get_pos", "get_text", "hovered", "get_mouse_axis"}},
        {"time", {"timer", "ticks", "timer_factor", "tick_factor", "delta", "fps", "date", "seconds", "get_scale", "cooldown"}},
        {"list", {"length", "contains", "index_of", "random_item", "last_item", "remove_last", "join"}},
    };
    auto it = reporters.find(U(category));
    if (it == reporters.end()) return false;
    std::string c = U(command);
    return std::find(it->second.begin(), it->second.end(), c) != it->second.end();
}

/// The block palette's label size: Glue3D's 0.45, or smaller so a long name stays inside its block.
double Glue3DSprite::PaletteLabelScale(const Value& label)
{
    // the same letter widths Glue3D's text drawing uses (stage units at size 1)
    double width = 0;
    for (char16_t c : label.Str().view())
    {
        double w = 9;
        switch (c)
        {
        case u'e': w += 0.64; break;
        case u'a': w += 1.5; break;
        case u'm': w += 6.5; break;
        case u'o': case u'd': w += 2; break;
        case u'r': w += 1; break;
        case u'i': case u'n': w -= 1; break;
        case u'l': w -= 0.5; break;
        case u'w': w += 5; break;
        default: break;
        }
        width += w;
    }
    constexpr double Normal = 0.45, Room = 68; // the block has room for about 68 stage units of text
    return width * Normal > Room ? Room / width : Normal;
}

/// How far (stage units) the block palette's blocks reach past the bottom of the palette.
double Glue3DSprite::PaletteOverflow()
{
    // blocks are 30 apart and about 5.5 of them fit
    double start = stage.autocomplete.IndexOf(stage.editorModelSidebar.Str() + u".start"_s);
    double end = stage.autocomplete.IndexOf(stage.editorModelSidebar.Str() + u".end"_s);
    double blocks = start > 0 && end > start ? std::floor((end - start - 1) / 3) : 0;
    return std::max(0.0, (blocks - 5.5) * 30);
}

/// The palette's scrollbar goes 0..100 (Glue3D's own range): at the end the last block is at the bottom.
double Glue3DSprite::PaletteScrollStep() { return PaletteOverflow() / 100; }

/// How far one mouse-wheel notch moves the palette's scrollbar: at least one block (30 stage units), however few
/// blocks there are. (Glue3D always moved it 5 of its 100 steps, so a short category took 20 notches.)
double Glue3DSprite::PaletteWheelStep()
{
    double step = PaletteScrollStep();
    return step > 0 ? std::max(5.0, 30 / step) : 100;
}

bool Glue3DSprite::PaletteShown()
{
    return Eq(stage.editorTab, u"script"_s) && Eq(stage.textEditor, 0) && Eq(stage.scriptLoaded, 1);
}

/// Where the left sidebar's scrollbar ends. For the block palette the bar is only as long as there is to scroll
/// (at least a short stub), so dragging it moves the blocks as far as the handle moves. Before, a category that
/// scrolled a few units still had the full-length bar, and dragging its handle to the middle barely moved it.
double Glue3DSprite::SidebarScrollEnd(double from, double to)
{
    if (!PaletteShown()) return to;
    double length = std::clamp(PaletteOverflow(), 25.0, from - to);
    return from - length;
}

// ------------------------------------------------------------------------------------------------
// Tests: run a script on one object without starting the whole game (automation "runscript").
// ------------------------------------------------------------------------------------------------

void Glue3DSprite::TestRunScript(const std::string& object, const std::vector<std::string>& lines)
{
    std::string script = "zz_test_" + object;
    int at = (int)stage.loadedScripts.IndexOf(T(script + ".start"));
    if (at > 0) // replace an earlier test script
    {
        int end = (int)stage.loadedScripts.IndexOf(T(script + ".end"));
        for (int i = end; i >= at; i--) stage.loadedScripts.Delete(i);
    }
    stage.loadedScripts.Add(T(script + ".start"));
    for (auto& l : lines) stage.loadedScripts.Add(T(l));
    stage.loadedScripts.Add(T(script + ".end"));
    int obj = (int)stage.scene.IndexOf(T(object + ".start")) - 1;
    if (obj < 1) return;
    stage.scene.Replace(obj + 29, T(script));
    stage.scene.Replace(obj + 31, 0);
    stage.scene.Replace(obj + 35, 0);
    stage.scene.Replace(obj + 37, 0);
    if (std::find(testObjects_.begin(), testObjects_.end(), object) == testObjects_.end()) testObjects_.push_back(object);
}

void Glue3DSprite::TestStep()
{
    for (size_t t = 0; t < testObjects_.size();)
    {
        if (TestStepObject(testObjects_[t])) t++;
        else testObjects_.erase(testObjects_.begin() + t);
    }
}

/// One frame of one test object's script; false once it has finished.
bool Glue3DSprite::TestStepObject(const std::string& name)
{
    int obj = (int)stage.scene.IndexOf(T(name + ".start")) - 1;
    if (obj < 1 || Eq(stage.scene.Item(obj + 29), u""_s)) return false;
    // what Glue3dExecute does for one object
    stage.objectIndex = obj;
    stage.scriptExit = 0;
    stage.scriptHeader = stage.loadedScripts.IndexOf(stage.scene.Item(obj + 29).Str() + u".start"_s);
    stage.scriptEnd = stage.loadedScripts.IndexOf(stage.scene.Item(obj + 29).Str() + u".end"_s);
    if (stage.timeClockSpeed.Num() <= 0) stage.timeClockSpeed = 1;
    for (int i = 0; i < 40; i++)
    {
        stage.timeScriptclock = (stage.scriptEnd.Num() - stage.scriptHeader.Num() - 1) * stage.timeClockSpeed.Num();
        if (!Eq(stage.scriptExit, 0) || Eq(stage.scene.Item(obj + 31), u"halt"_s)) return false;
        if (ScriptPaused()) return true;
        RunLineMainObjectScript(stage.scene.Item(obj + 31), stage.scene.Item(obj + 3), stage.scene.Item(obj + 29));
        if (Eq(stage.scriptExit, 0) && Lt(stage.scene.Item(obj + 35), 0.0125) && !Eq(stage.scene.Item(obj + 31), u"halt"_s))
        {
            stage.scene.Replace(obj + 31, stage.scene.Item(obj + 31).Num() + 1);
            stage.scene.Replace(obj + 37, 0);
        }
    }
    return true;
}

} // namespace Glue3D
