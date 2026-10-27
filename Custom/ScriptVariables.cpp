// Hand-written: variables (the editor and built games both run these).
//
//   global / local / const / saved var    variables every object sees, function-only ones, fixed ones, saved ones
//   math on one line                      var d = (var x - var px) ^ val 2 + val 1   (also inside if / while)
//   shortcuts                             ++ -- toggle .= ?= min= max= clamp swap
//   arrays                                var slot[1] = str sword,  var w = var slot[var i],  var.push / pop …
//   var.* blocks                          get / set / exists / delete / names / copy_from / changed / previous /
//                                         type / is_number / round_to / strict / length / push / pop / clear_array
//
// Every "var …", "global …", "local …", "const …" and "saved var …" line comes here (from SystemsCommand), so the
// whole line is read here, including what Glue3D's own variable block did ("var x = val 5", "+=", "-=", "*=").
#include <algorithm>
#include <cmath>
#include <sstream>

#include "BuildFlavor.h"
#include "Engine.h"
#include "Glue3DSprite.g.h"

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

/// The variables every object has; var.names and var.copy_from leave them out.
bool BuiltIn(const std::string& k)
{
    static const char* names[] = {"name", "type", "model", "update", "x", "y", "z", "rx", "ry", "rz", "sx", "sy", "sz",
                                  "script", "clock", "libs", "wait_clock", "temp", "bone", "data_return"};
    for (const char* n : names)
        if (k == n) return true;
    return false;
}

bool IsTypeWord(const std::string& w) { return w == "val" || w == "var" || w == "str" || w == "global" || w == "local"; }

bool IsNumberText(const std::string& s)
{
    if (s.empty()) return false;
    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    return end && *end == 0 && s.find_first_not_of(" \t") != std::string::npos;
}

std::string NumberText(double d) { return U(Value(d)); }

} // namespace

// ------------------------------------------------------------------------------------------------
// Tokens and the expression / condition parser
// ------------------------------------------------------------------------------------------------

/// Splits a piece of script into tokens: words, typed values, operators, brackets and block calls.
std::vector<Glue3DSprite::Token> Glue3DSprite::Tokenize(const std::string& s)
{
    std::vector<Token> out;
    size_t i = 0, n = s.size();
    auto isOp = [](char c) { return std::string("()[],+*/%^").find(c) != std::string::npos; };
    auto isCmp = [](char c) { return c == '=' || c == '!' || c == '<' || c == '>'; };
    while (i < n)
    {
        char c = s[i];
        if (c == ' ' || c == '\t')
        {
            i++;
            continue;
        }
        std::string prev = out.empty() ? "" : out.back().Text;
        bool afterType = !out.empty() && out.back().Kind == Token::Word && IsTypeWord(prev);
        if (afterType)
        {
            // the value after val / str / var / global / local: up to a space, comma or closing bracket
            // (var names also stop at "[" for arrays)
            size_t j = i;
            bool name = prev == "var" || prev == "global" || prev == "local";
            while (j < n && s[j] != ' ' && s[j] != ',' && s[j] != ')' && s[j] != ']' && !(name && s[j] == '[')) j++;
            out.push_back({Token::Value, s.substr(i, j - i)});
            i = j;
            continue;
        }
        if (isCmp(c))
        {
            size_t j = i + 1;
            if (j < n && s[j] == '=') j++;
            out.push_back({Token::Op, s.substr(i, j - i)});
            i = j;
            continue;
        }
        if (c == '-' || isOp(c))
        {
            out.push_back({Token::Op, std::string(1, c)});
            i++;
            continue;
        }
        size_t j = i;
        while (j < n && s[j] != ' ' && s[j] != '\t' && !isOp(s[j]) && !isCmp(s[j]) && s[j] != '-') j++;
        std::string word = s.substr(i, j - i);
        // a block that answers: category.command( … )
        if (word.find('.') != std::string::npos && j < n && s[j] == '(' && !IsNumberText(word))
        {
            int depth = 0;
            size_t k = j;
            for (; k < n; k++)
            {
                if (s[k] == '(') depth++;
                else if (s[k] == ')' && --depth == 0) break;
            }
            out.push_back({Token::Call, s.substr(i, std::min(k + 1, n) - i)});
            i = std::min(k + 1, n);
            continue;
        }
        out.push_back({Token::Word, word});
        i = j;
    }
    return out;
}

struct Glue3DSprite::Parser
{
    Glue3DSprite& g;
    std::vector<Token> t;
    size_t p = 0;
    bool failed = false;
    bool waiting = false; // a block inside is waiting (terminal.input): try again next time

    bool At(const char* s) const { return p < t.size() && t[p].Kind != Token::Value && t[p].Text == s; }
    bool End() const { return p >= t.size(); }
    bool Take(const char* s)
    {
        if (!At(s)) return false;
        p++;
        return true;
    }

    // ---- conditions ----
    bool Cond()
    {
        bool v = AndC();
        while (!failed && (At("or") || At("xor") || At("nand") || At("nor")))
        {
            std::string op = t[p++].Text;
            bool b = AndC();
            if (op == "or") v = v || b;
            else if (op == "xor") v = v != b;
            else if (op == "nand") v = !(v && b);
            else v = !(v || b);
        }
        return v;
    }
    bool AndC()
    {
        bool v = NotC();
        while (!failed && Take("and"))
        {
            bool b = NotC();
            v = v && b;
        }
        return v;
    }
    bool NotC()
    {
        if (Take("not")) return !NotC();
        return Compare();
    }
    static bool IsCmp(const std::string& s)
    {
        return s == "=" || s == "==" || s == "!=" || s == ">" || s == "<" || s == ">=" || s == "<=" || s == "equals" ||
               s == "not_equals" || s == "greater_than" || s == "less_than" || s == "greater_equal" || s == "less_equal" ||
               s == "contains";
    }
    bool Compare()
    {
        // ( condition ) — unless the brackets are part of a sum, like (var a + val 1) > val 3
        if (At("("))
        {
            size_t save = p;
            bool saveFailed = failed;
            p++;
            bool v = Cond();
            if (!failed && Take(")") && (End() || At(")") || At("and") || At("or") || At("xor") || At("nand") || At("nor")))
                return v;
            p = save;
            failed = saveFailed;
        }
        Value a = Expr();
        if (!End() && t[p].Kind != Token::Value && t[p].Kind != Token::Call && IsCmp(t[p].Text))
        {
            std::string op = t[p++].Text;
            Value b = Expr();
            if (op == "=" || op == "==" || op == "equals") return Eq(a, b);
            if (op == "!=" || op == "not_equals") return !Eq(a, b);
            if (op == ">" || op == "greater_than") return Gt(a, b);
            if (op == "<" || op == "less_than") return Lt(a, b);
            if (op == ">=" || op == "greater_equal") return !Lt(a, b);
            if (op == "<=" || op == "less_equal") return !Gt(a, b);
            return a.Str().Lower().view().find(b.Str().Lower().view()) != std::u16string_view::npos; // contains
        }
        return Eq(a, u"true"_s); // a value on its own: is it true?
    }

    // ---- math: + - then * / % then ^ ----
    Value Expr()
    {
        Value v = Term();
        while (!failed && (At("+") || At("-")))
        {
            bool plus = t[p++].Text == "+";
            Value b = Term();
            v = plus ? v.Num() + b.Num() : v.Num() - b.Num();
        }
        return v;
    }
    Value Term()
    {
        Value v = Unary();
        while (!failed && (At("*") || At("/") || At("%")))
        {
            std::string op = t[p++].Text;
            Value b = Unary();
            if (op == "*") v = v.Num() * b.Num();
            else if (op == "/") v = Nz(v.Num() / b.Num());
            else v = Mod(v.Num(), b.Num());
        }
        return v;
    }
    Value Unary()
    {
        if (Take("-")) return -Unary().Num();
        Value v = Primary();
        if (!failed && Take("^"))
        {
            Value e = Unary(); // right to left: 2 ^ 3 ^ 2 = 2 ^ 9
            v = std::pow(v.Num(), e.Num());
        }
        return v;
    }
    Value Primary()
    {
        if (End())
        {
            failed = true;
            return Value();
        }
        if (Take("("))
        {
            Value v = Expr();
            if (!Take(")")) failed = true;
            return v;
        }
        const Token& k = t[p];
        if (k.Kind == Token::Call)
        {
            p++;
            Value v = g.RunAnswerBlock(k.Text, waiting);
            return v;
        }
        if (k.Kind == Token::Word)
        {
            p++;
            const std::string& w = k.Text;
            if (w == "data_return") return g.ReadData(g.stage.scriptObject, u"data_return"_s);
            if (IsTypeWord(w))
            {
                if (End() || t[p].Kind != Token::Value)
                {
                    if (w == "str") return u""_s; // "str" with nothing after it: empty text
                    failed = true;
                    return Value();
                }
                std::string value = t[p++].Text;
                if (w == "val") return IsNumberText(value) ? Value(std::strtod(value.c_str(), nullptr)) : T(value);
                if (w == "str") return T(value);
                if (w == "global") return g.ReadGlobal(value);
                // var / local, maybe an array item: var slot[var i]
                if (Take("["))
                {
                    Value index = Expr();
                    if (!Take("]")) failed = true;
                    value = value + "_" + NumberText(std::floor(index.Num()));
                }
                return g.ReadVar(value);
            }
            if (IsNumberText(w)) return std::strtod(w.c_str(), nullptr);
            if (w == "true" || w == "false") return T(w);
            failed = true; // an unknown word
            return Value();
        }
        failed = true;
        return Value();
    }
};

/// Runs a block that answers (inside a line) and gives its answer.
Value Glue3DSprite::RunAnswerBlock(const std::string& text, bool& waiting)
{
    RunBlockLine(text);
    if (!Lt(stage.scene.Item(stage.objectIndex.Num() + 35), 0.0125)) waiting = true; // it waits (terminal.input)
    return ReadData(stage.scriptObject, u"data_return"_s);
}

bool Glue3DSprite::EvalExpression(const std::string& text, Value& out, bool& waiting)
{
    Parser ps{*this, Tokenize(text)};
    out = ps.Expr();
    waiting = ps.waiting;
    return !ps.failed && ps.End();
}

bool Glue3DSprite::EvalConditionText(const std::string& text, bool& out)
{
    Parser ps{*this, Tokenize(text)};
    out = ps.Cond();
    return !ps.failed && ps.End();
}

/// The text inside a line's outer brackets: "if (var a > val 1)" -> "var a > val 1".
static std::string Inside(const std::string& line)
{
    size_t a = line.find('('), b = line.rfind(')');
    if (a == std::string::npos || b == std::string::npos || b <= a) return "";
    return line.substr(a + 1, b - a - 1);
}

/// if / while / wait_until conditions, read from the script line itself (Glue3D's Operation asks this first).
bool Glue3DSprite::LineCondition(bool& result)
{
    std::string line = Trim(U(stage.scriptLine));
    std::string first = line.substr(0, line.find_first_of(" ("));
    if (first == "endwhile")
    {
        int start = FindBlockStart((int)stage.scriptReadindex.Num(), "while", "endwhile");
        if (!start) return false;
        line = Trim(U(stage.loadedScripts.Item(start)));
        first = "while";
    }
    if (first != "if" && first != "while" && first != "wait_until") return false;
    return EvalConditionText(Inside(line), result);
}

// ------------------------------------------------------------------------------------------------
// Reading and writing
// ------------------------------------------------------------------------------------------------

bool Glue3DSprite::FindVar(const std::string& name, Value& out)
{
    int idx = (int)stage.scene.IndexOf(stage.scriptObject.Str() + u".start"_s) - 1;
    if (idx < 1) return false;
    int len = (int)stage.scene.Item(idx).Num();
    Value key = T(name);
    for (int i = idx + 2; i + 1 < idx + len; i += 2)
        if (Eq(stage.scene.Item(i), key))
        {
            out = stage.scene.Item(i + 1);
            return true;
        }
    return false;
}

/// Turns "slot[2]" or "slot[i]" (a number or a variable name) into the stored name "slot_2".
std::string Glue3DSprite::ArrayName(const std::string& name)
{
    size_t a = name.find('['), b = name.rfind(']');
    if (a == std::string::npos || b == std::string::npos || b < a) return name;
    std::string base = name.substr(0, a), index = Trim(name.substr(a + 1, b - a - 1));
    Value v;
    bool waiting = false;
    if (!IsNumberText(index) && !EvalExpression(index, v, waiting))
    {
        Value tmp;
        v = FindVar(index, tmp) ? tmp : Value(0);
    }
    else if (IsNumberText(index))
        v = std::strtod(index.c_str(), nullptr);
    return base + "_" + NumberText(std::floor(v.Num()));
}

Value Glue3DSprite::ReadVar(const std::string& rawName)
{
    std::string name = ArrayName(rawName);
    Value v;
    if (FindVar(name, v)) return v;
    if (strictVars_)
    {
        std::string key = U(stage.scriptObject) + "#" + name + "#" + std::to_string(ScriptLineNumber());
        if (strictWarned_.insert(key).second)
            PrintLevel(2, "warning: " + SourceText() + " no variable \"" + name + "\" (a typo?)");
    }
    return u"null"_s; // what Glue3D's own blocks give for a missing variable
}

Value Glue3DSprite::ReadGlobal(const std::string& name)
{
    auto it = globals_.find(name);
    if (it != globals_.end()) return it->second;
    if (strictVars_)
    {
        std::string key = "global#" + name + "#" + std::to_string(ScriptLineNumber());
        if (strictWarned_.insert(key).second)
            PrintLevel(2, "warning: " + SourceText() + " no global variable \"" + name + "\" (a typo?)");
    }
    return Value(0);
}

void Glue3DSprite::DeleteVar(const Value& object, const std::string& name)
{
    int idx = (int)stage.scene.IndexOf(object.Str() + u".start"_s) - 1;
    if (idx < 1) return;
    int len = (int)stage.scene.Item(idx).Num();
    Value key = T(name);
    for (int i = idx + 2; i + 1 < idx + len; i += 2)
        if (Eq(stage.scene.Item(i), key))
        {
            stage.scene.Delete(i);
            stage.scene.Delete(i);
            stage.scene.Replace(idx, len - 2);
            return;
        }
}

/// "object@script" for the running script: where its function frames are kept.
std::string Glue3DSprite::FrameKey() { return U(stage.scriptObject) + "@" + U(stage.scriptName); }

/// Inside a function, a variable made with "local" (and arg1, arg2…) gets its old value back when the function ends.
void Glue3DSprite::MakeLocal(const std::string& name)
{
    auto it = callFrames_.find(FrameKey());
    if (it == callFrames_.end() || it->second.empty()) return; // not in a function: a normal variable
    auto& frame = it->second.back();
    if (frame.count(name)) return;
    Value old;
    frame[name] = FindVar(name, old) ? std::optional<Value>(old) : std::nullopt;
}

void Glue3DSprite::PushFrame() { callFrames_[FrameKey()].emplace_back(); }

void Glue3DSprite::PopFrame()
{
    auto it = callFrames_.find(FrameKey());
    if (it == callFrames_.end() || it->second.empty()) return;
    for (auto& [name, old] : it->second.back())
    {
        if (old) WriteData(stage.scriptObject, T(name), *old);
        else DeleteVar(stage.scriptObject, name);
    }
    it->second.pop_back();
}

/// After Glue3D reads a block's values (DataRecall): "global x", "local x" and "var slot[2]" give their values too.
void Glue3DSprite::ApplyValueTypes()
{
    Value* vals[] = {&stage.dataVal1, &stage.dataVal2, &stage.dataVal3, &stage.dataVal4};
    for (int i = 0; i < 4; i++)
    {
        Value type = stage.decodedElements.Item(3 + 3 * i);
        std::string value = U(stage.decodedElements.Item(4 + 3 * i));
        if (Eq(type, u"global"_s)) *vals[i] = ReadGlobal(value);
        else if (Eq(type, u"local"_s)) *vals[i] = ReadVar(value);
        else if (Eq(type, u"var"_s) && value.find('[') != std::string::npos) *vals[i] = ReadVar(value);
    }
}

// ------------------------------------------------------------------------------------------------
// var / global / local / const / saved var lines
// ------------------------------------------------------------------------------------------------

bool Glue3DSprite::AssignCommand()
{
    std::string line = Trim(U(stage.scriptLine));
    std::string kind = line.substr(0, line.find_first_of(" .(\t"));
    if (kind == "var" && line.compare(0, 4, "var.") == 0) return VarBlocks();
    bool saved = false;
    if (kind == "saved")
    {
        std::istringstream in(line.substr(5));
        std::string v;
        in >> v;
        if (v != "var") return false;
        saved = true;
    }
    if (kind != "var" && kind != "global" && kind != "local" && kind != "const" && !saved) return false;

    // the target: a name, maybe with [index] (which may hold spaces)
    size_t pos = line.find(kind) + kind.size();
    if (saved) pos = line.find("var", pos) + 3;
    std::string rest = Trim(line.substr(pos));
    size_t j = 0;
    int depth = 0;
    while (j < rest.size() && (depth > 0 || (rest[j] != ' ' && rest[j] != '\t')))
    {
        if (rest[j] == '[') depth++;
        else if (rest[j] == ']') depth--;
        j++;
    }
    std::string target = rest.substr(0, j);
    rest = Trim(rest.substr(j));
    size_t sp = rest.find_first_of(" \t");
    std::string op = rest.substr(0, sp);
    std::string rhs = sp == std::string::npos ? "" : Trim(rest.substr(sp));
    if (target.empty() || op.empty())
    {
        PrintLevel(3, "error: " + SourceText() + " can't read \"" + line + "\"");
        return true;
    }

    bool global = kind == "global";
    // arrays: var slot[var i] = …
    std::string name = target;
    if (target.find('[') != std::string::npos)
    {
        size_t a = target.find('[');
        Value index;
        bool waiting = false;
        if (!EvalExpression(target.substr(a + 1, target.rfind(']') - a - 1), index, waiting))
        {
            std::string inner = Trim(target.substr(a + 1, target.rfind(']') - a - 1));
            Value tmp;
            index = IsNumberText(inner) ? Value(std::strtod(inner.c_str(), nullptr)) : (FindVar(inner, tmp) ? tmp : Value(0));
        }
        name = target.substr(0, a) + "_" + NumberText(std::floor(index.Num()));
    }

    // the object's own bookkeeping can't be changed from a script (changing "name" lost the object)
    static const char* locked[] = {"name", "type", "update", "script", "clock", "libs", "wait_clock", "temp", "bone"};
    if (!global)
        for (const char* l : locked)
            if (name == l)
            {
                PrintLevel(3, "error: " + SourceText() + " \"" + name + "\" is the object's own; pick another variable name");
                return true;
            }

    // const: once set, it can't change
    std::string constKey = (global ? std::string("global#") : U(stage.scriptObject) + "#") + name;
    if (constants_.count(constKey))
    {
        PrintLevel(3, "error: " + SourceText() + " " + name + " is a constant; it can't change");
        return true;
    }

    auto current = [&](Value& v) {
        if (global)
        {
            auto it = globals_.find(name);
            if (it == globals_.end()) return false;
            v = it->second;
            return true;
        }
        return FindVar(name, v);
    };
    auto store = [&](const Value& v) {
        if (global) globals_[name] = v;
        else WriteData(stage.scriptObject, T(name), v);
    };
    auto value = [&](const std::string& text, Value& v) {
        bool waiting = false;
        if (text.empty())
        {
            PrintLevel(3, "error: " + SourceText() + " nothing after " + op);
            return false;
        }
        if (!EvalExpression(text, v, waiting))
        {
            if (!waiting) PrintLevel(3, "error: " + SourceText() + " can't work out \"" + text + "\"");
            return false;
        }
        return !waiting; // a block that waits answers later: this line runs again
    };

    if (kind == "local") MakeLocal(name);

    Value cur, v;
    bool exists = current(cur);
    double a = exists ? cur.Num() : 0;
    if (op == "=")
    {
        // Glue3D's own rule: "var x = data_return" does nothing while there is no answer yet
        if (rhs == "data_return" && Eq(ReadData(stage.scriptObject, u"data_return"_s), u"null"_s)) return true;
        if (!value(rhs, v)) return true;
        if (saved)
        {
            // saved var: the saved value wins over the starting value; changes are saved by themselves
            Value s;
            if (SaveGet(name, s)) v = s;
            else SaveSet(name, v);
            savedVars_[U(stage.scriptObject) + "#" + name] = U(v);
        }
        store(v);
        if (kind == "const") constants_.insert(constKey);
        return true;
    }
    if (op == "++" || op == "--")
    {
        store(a + (op == "++" ? 1 : -1));
        return true;
    }
    if (op == "toggle")
    {
        store(Bool(!Eq(cur, u"true"_s)));
        return true;
    }
    if (op == "?=")
    {
        if (exists) return true; // only when it doesn't exist yet
        if (value(rhs, v)) store(v);
        return true;
    }
    if (op == ".=")
    {
        if (!value(rhs, v)) return true;
        store(T((exists ? U(cur) : "") + U(v)));
        return true;
    }
    if (op == "swap")
    {
        // var a swap var b
        std::istringstream r(rhs);
        std::string type, other;
        r >> type >> other;
        if (type != "var" && type != "global")
        {
            PrintLevel(3, "error: " + SourceText() + " swap needs a variable: var a swap var b");
            return true;
        }
        Value b;
        bool otherGlobal = type == "global";
        if (otherGlobal) b = ReadGlobal(other);
        else b = ReadVar(other);
        Value mine = exists ? cur : Value(0);
        store(b);
        if (otherGlobal) globals_[other] = mine;
        else WriteData(stage.scriptObject, T(ArrayName(other)), mine);
        return true;
    }
    if (op == "clamp")
    {
        // var hp clamp (val 0, val 100)
        std::string inside = Inside("x" + rhs);
        size_t comma = std::string::npos;
        int d = 0;
        for (size_t i = 0; i < inside.size(); i++)
        {
            if (inside[i] == '(' || inside[i] == '[') d++;
            else if (inside[i] == ')' || inside[i] == ']') d--;
            else if (inside[i] == ',' && d == 0) comma = i;
        }
        Value lo, hi;
        if (comma == std::string::npos || !value(inside.substr(0, comma), lo) || !value(inside.substr(comma + 1), hi)) return true;
        store(std::clamp(a, std::min(lo.Num(), hi.Num()), std::max(lo.Num(), hi.Num())));
        return true;
    }
    // number operators
    if (!value(rhs, v)) return true;
    double b = v.Num(), r;
    if (op == "+=") r = a + b;
    else if (op == "-=") r = a - b;
    else if (op == "*=") r = a * b;
    else if (op == "/=") r = Nz(a / b);
    else if (op == "%=") r = Mod(a, b);
    else if (op == "^=") r = std::pow(a, b);
    else if (op == "min=") r = std::min(a, b);
    else if (op == "max=") r = std::max(a, b);
    else
    {
        PrintLevel(3, "error: " + SourceText() + " unknown operator \"" + op + "\"");
        return true;
    }
    store(r);
    return true;
}

// ------------------------------------------------------------------------------------------------
// var.* blocks
// ------------------------------------------------------------------------------------------------

bool Glue3DSprite::VarBlocks()
{
    // decoded like other blocks: var . command ( … )
    String c = Cmd();
    const Value& me = stage.scriptObject;
    auto name = [&]() { return U(Arg(0)); };

    if (c == u"get") return SetReturn(ReadVar(name())), true;
    if (c == u"set")
    {
        std::string n = name();
        if (constants_.count(U(me) + "#" + n))
        {
            PrintLevel(3, "error: " + SourceText() + " " + n + " is a constant; it can't change");
            return true;
        }
        WriteData(me, T(ArrayName(n)), Arg(1));
        return true;
    }
    if (c == u"exists")
    {
        Value v;
        return SetReturn(Bool(FindVar(ArrayName(name()), v))), true;
    }
    if (c == u"delete") return DeleteVar(me, ArrayName(name())), true;
    if (c == u"names" || c == u"copy_from")
    {
        std::string from = c == u"names" ? U(me) : U(Arg(0));
        std::vector<std::pair<std::string, Value>> vars;
        for (auto& o : ObjectRecords())
            if (o.Name == from)
                for (int i = o.Index + 2; i + 1 < o.Index + o.Length; i += 2)
                {
                    std::string k = U(stage.scene.Item(i));
                    if (!BuiltIn(k)) vars.push_back({k, stage.scene.Item(i + 1)});
                }
        if (c == u"names")
        {
            std::string out;
            for (auto& [k, v] : vars) out += k + ",";
            SetReturn(T(out));
        }
        else
            for (auto& [k, v] : vars) WriteData(me, T(k), v);
        return true;
    }
    if (c == u"changed" || c == u"previous")
    {
        std::string n = ArrayName(name());
        std::string key = U(me) + "#" + n;
        Value now = ReadVar(n);
        auto& w = changes_[key];
        if (c == u"changed")
        {
            bool changed = w.Seen && !Eq(w.Last, now);
            if (changed) w.Previous = w.Last;
            w.Last = now;
            w.Seen = true;
            SetReturn(Bool(changed));
        }
        else
            SetReturn(w.Seen ? w.Previous : now);
        return true;
    }
    if (c == u"type")
    {
        Value v = Arg(0);
        std::string s = U(v);
        SetReturn(T(Eq(v, u"null"_s) ? "none" : s == "true" || s == "false" ? "bool" : (v.IsNumber() || IsNumberText(s)) ? "number" : "text"));
        return true;
    }
    if (c == u"is_number")
    {
        Value v = Arg(0);
        return SetReturn(Bool(v.IsNumber() || IsNumberText(U(v)))), true;
    }
    if (c == u"round_to")
    {
        double x = Arg(0).Num(), p = std::pow(10.0, std::clamp(std::floor(Arg(1).Num()), 0.0, 12.0));
        return SetReturn(std::round(x * p) / p), true;
    }
    if (c == u"strict") return strictVars_ = !Eq(Arg(0), u"false"_s), true;
    // arrays
    if (c == u"length" || c == u"push" || c == u"pop" || c == u"clear_array")
    {
        std::string base = name();
        int count = 0;
        Value v;
        while (FindVar(base + "_" + std::to_string(count + 1), v)) count++;
        if (c == u"length") SetReturn(count);
        else if (c == u"push") WriteData(me, T(base + "_" + std::to_string(count + 1)), Arg(1));
        else if (c == u"pop")
        {
            if (count == 0) SetReturn(u""_s);
            else
            {
                std::string last = base + "_" + std::to_string(count);
                FindVar(last, v);
                SetReturn(v);
                DeleteVar(me, last);
            }
        }
        else
            for (int i = 1; i <= count; i++) DeleteVar(me, base + "_" + std::to_string(i));
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------------------------------
// Every frame: saved vars write their changes to save.json
// ------------------------------------------------------------------------------------------------

void Glue3DSprite::VariablesUpdate()
{
    for (auto& [key, last] : savedVars_)
    {
        size_t hash = key.find('#');
        std::string object = key.substr(0, hash), name = key.substr(hash + 1);
        Value saved = stage.scriptObject;
        stage.scriptObject = T(object);
        Value v;
        bool found = ObjectExists(stage.scriptObject) && FindVar(name, v);
        stage.scriptObject = saved;
        if (found && U(v) != last)
        {
            last = U(v);
            SaveSet(name, v);
        }
    }
}

void Glue3DSprite::ResetVariableTools()
{
    globals_.clear();
    constants_.clear();
    changes_.clear();
    savedVars_.clear();
    callFrames_.clear();
    strictVars_ = false;
    strictWarned_.clear();
}

// ------------------------------------------------------------------------------------------------
// The editor's variables panel: the selected object's variables, live, while the game runs
// ------------------------------------------------------------------------------------------------

void Glue3DSprite::DrawVariablesPanel(Rect r, Rect view)
{
    if (!BuildFlavor::IsEditor || !Eq(stage.allRun, 1)) return;
    std::string name = InspectedObject();
    if (name.empty()) return;
    std::vector<std::string> lines;
    for (auto& o : ObjectRecords())
        if (o.Name == name)
        {
            for (int i = o.Index + 2; i + 1 < o.Index + o.Length; i += 2)
            {
                std::string k = U(stage.scene.Item(i));
                if (BuiltIn(k) && k != "x" && k != "y" && k != "z" && k != "rx") continue;
                std::string v = U(stage.scene.Item(i + 1));
                if (v.size() > 24) v = v.substr(0, 22) + "..";
                lines.push_back(k + " = " + v);
            }
            for (auto& [g, v] : globals_) lines.push_back("global " + g + " = " + U(v));
        }
    if (lines.empty()) return;
    if (lines.size() > 22) lines.resize(22);
    float k = r.w / 480.0f, size = 7.5f * k, lineH = size + 2.5f * k, pad = 5 * k;
    auto& mon = GetEngine().monitors;
    float w = mon.Measure("variables of " + name, size);
    for (auto& l : lines) w = std::max(w, mon.Measure(l, size));
    Rect box{view.x + view.w - w - 3 * pad, view.y + pad, w + 2 * pad, (lines.size() + 1) * lineH + pad};
    gfx::DrawRectRounded(box, 0.1f, 6, {12, 14, 20, 185});
    mon.Text("variables of " + name, box.x + pad, box.y + pad / 2, size, {255, 210, 120, 255});
    for (size_t i = 0; i < lines.size(); i++) mon.Text(lines[i], box.x + pad, box.y + pad / 2 + (i + 1) * lineH, size, {225, 230, 240, 255});
}

} // namespace Glue3D
