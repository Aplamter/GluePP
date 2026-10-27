#!/usr/bin/env python3
"""
Scratch 3 project (.sb3, unzipped) -> readable C++20 for the Scratch runtime in GluePP/runtime.

Usage: python3 sb3_to_cpp.py <unzipped sb3 dir> <output Generated dir> <assets dir>
                             [--namespace NS] [--title T] [--rename "old=New"] [--members Class=path.h]

Writes:
  <out>/Stage.g.h, Stage.g.cpp            - the stage: global variables, lists, its scripts
  <out>/<Sprite>.g.h, <Sprite>.g*.cpp     - one class per sprite; custom blocks become methods
  <out>/Project.g.h, Project.g.cpp        - creates the targets with their starting state
  <assets>/initial-lists.json             - starting contents of every list (kept out of the code)

Scratch scripts become C++20 coroutines ("Script"): `co_yield NextFrame;` is where Scratch waits for the
next frame, and custom blocks that can pause are called with `co_await`.

--members Class=X.members.h  adds `#include "X.members.h"` inside that class, so hand-written members (declared in
that file, defined in your own .cpp files) become part of the generated class, like a C# partial class.
X.includes.h is included at the top of the header for the headers those members need.
"""
import json, math, os, re, sys

CPP_KEYWORDS = set("""alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t
char32_t class compl concept const consteval constexpr constinit const_cast continue co_await co_return co_yield
decltype default delete do double dynamic_cast else enum explicit export extern false float for friend goto if inline
int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected public register
reinterpret_cast requires return short signed sizeof static static_assert static_cast struct switch template this
thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor
xor_eq final override import module NULL EOF assert errno stdin stdout stderr offsetof min max near far TRUE FALSE
std scratch ops main""".split())

# Members of the runtime base classes (and helper functions) that generated names must not hide.
RESERVED_MEMBERS = set("""engine Costumes Sounds CurrentCostume Effects Volume SoundPitch SoundPan SoundVoices Disposed
IsStage Hats GetEngine GetCostume CostumeNumber CostumeName SetCostumeIndex SwitchCostume SetEffect ChangeEffect
ClearEffects OnVisualChange StartSound SetVolume ChangeVolume SetSoundEffect ChangeSoundEffect ClearSoundEffects
StopAllSounds MouseX MouseY MouseDown RightMouseDown LookDeltaX LookDeltaY KeyPressed Timer ResetTimer Broadcast
StopOtherScriptsInSprite StopAll PenClear ShowList HideList Wait BusyWait SoundIndex Name X Y Direction Size Visible
Rotation IsClone BaseStage PenIsDown PenColor PenSaturation PenBrightness PenTransparency PenSize PenR PenG PenB PenA
GoTo SetX SetY ChangeX ChangeY PointInDirection TurnRight TurnLeft SetSize ChangeSize Show Hide PenDown PenUp Stamp
SetPenSize ChangePenSize SetPenColor SetPenParam ChangePenParam TouchingMouse CreateClone DeleteThisClone CloneSelf
XPosition YPosition SizeReporter LimitPrecision SetOrChangePenParam UpdatePenColor stage
Num Str Bool Nz Compare Eq Lt Gt Mod Round Sin Cos Tan Asin Acos Atan Ln Log10 Exp Pow10 PickRandom RandomUnit
LetterOf Contains Clamp WrapClamp Value String Script Hat Target Engine Stage ScratchList NextFrame ValueFields
ListFields kNaN kInfinity kPi""".split())


def words(name):
    return [w for w in re.split(r"[^A-Za-z0-9]+", re.sub(r"([a-z0-9])([A-Z])", r"\1 \2", name)) if w]


def pascal(name, fallback="Block"):
    ws = words(name)
    s = "".join(w[0].upper() + w[1:] for w in ws) or fallback
    if s and s[0].isdigit():
        s = "_" + s
    return s


def camel(name, fallback="value"):
    ws = words(name)
    if not ws:
        return fallback
    s = ws[0][0].lower() + ws[0][1:] + "".join(w[0].upper() + w[1:] for w in ws[1:])
    if s[0].isdigit():
        s = "_" + s
    if s in CPP_KEYWORDS:
        s = s + "_"
    return s


class Names:
    """Hands out unique C++ identifiers."""

    def __init__(self, taken=()):
        self.used = set(taken)

    def get(self, base):
        name, n = base, 2
        while name in self.used or name in RESERVED_MEMBERS or name in CPP_KEYWORDS:
            name = f"{base}{n}"
            n += 1
        self.used.add(name)
        return name


def cpp_string(s):
    """A Scratch string literal: u"..."_s (UTF-16, like JavaScript). The source stays plain ASCII."""
    out = ['u"']
    hex_pending = False
    for o in utf16_units(s):
        c = chr(o) if o < 0xD800 or o > 0xDFFF else None
        if hex_pending and c is not None and c in "0123456789abcdefABCDEF":
            out.append('" u"')  # end the hex escape before a hex digit
        hex_pending = False
        if c == '"': out.append('\\"')
        elif c == "\\": out.append("\\\\")
        elif c == "\n": out.append("\\n")
        elif c == "\r": out.append("\\r")
        elif c == "\t": out.append("\\t")
        elif c == "?": out.append("\\?")
        elif c is not None and 32 <= o < 127: out.append(c)
        else:
            out.append("\\x%04x" % o)
            hex_pending = True
    out.append('"_s')
    return "".join(out)


def utf16_units(s):
    data = s.encode("utf-16-le", "surrogatepass")
    return [data[i] | (data[i + 1] << 8) for i in range(0, len(data), 2)]


def js_number_to_string(d):
    """Python port of JavaScript Number#toString (matches Js::NumberToString in the runtime)."""
    if d != d: return "NaN"
    if d == math.inf: return "Infinity"
    if d == -math.inf: return "-Infinity"
    if d == 0: return "0"
    r = repr(float(d))
    neg = r.startswith("-")
    if neg: r = r[1:]
    mant, exp = (r.split("e") + ["0"])[:2] if "e" in r else (r, "0")
    exp = int(exp)
    if "." in mant:
        point = mant.index("."); digits = mant.replace(".", "")
    else:
        point = len(mant); digits = mant
    lead = 0
    while lead < len(digits) - 1 and digits[lead] == "0":
        lead += 1; point -= 1
    digits = digits[lead:].rstrip("0") or "0"
    k = len(digits); n = point + exp
    if k <= n <= 21: body = digits + "0" * (n - k)
    elif 0 < n <= 21: body = digits[:n] + "." + digits[n:]
    elif -6 < n <= 0: body = "0." + "0" * (-n) + digits
    else:
        e = n - 1
        es = ("+" if e >= 0 else "-") + str(abs(e))
        body = digits + "e" + es if k == 1 else digits[0] + "." + digits[1:] + "e" + es
    return "-" + body if neg else body


def js_to_number(s):
    """JavaScript Number(string); returns None for NaN."""
    t = s.strip(" \t\n\r\v\f ﻿")
    if t == "": return 0.0
    if t in ("Infinity", "+Infinity"): return math.inf
    if t == "-Infinity": return -math.inf
    if re.fullmatch(r"0[xX][0-9a-fA-F]+", t): return float(int(t[2:], 16))
    if re.fullmatch(r"0[oO][0-7]+", t): return float(int(t[2:], 8))
    if re.fullmatch(r"0[bB][01]+", t): return float(int(t[2:], 2))
    if re.fullmatch(r"[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?", t): return float(t)
    return None


def exact_number(s):
    """Text that round-trips exactly through a number (so it can be stored as one)."""
    if isinstance(s, bool): return None
    if isinstance(s, (int, float)): return float(s)
    if not isinstance(s, str): return None
    n = js_to_number(s)
    if n is None: return None
    return n if js_number_to_string(n) == s else None


def cpp_number(n):
    if n != n: return "kNaN"
    if n == math.inf: return "kInfinity"
    if n == -math.inf: return "-kInfinity"
    if n == int(n) and abs(n) < 2 ** 31:
        return str(int(n)) if n != 0 or math.copysign(1, n) > 0 else "-0.0"
    if n == int(n) and abs(n) < 1e15:
        return str(int(n)) + ".0"
    s = repr(n)
    return s if ("." in s or "e" in s) else s + ".0"


def js_to_bool(s):
    return not (s == "" or s == "0" or s.lower() == "false")


class E:
    """A generated C++ expression and its static type: N number, Nq number-maybe-NaN, S text, B boolean, V Value."""
    __slots__ = ("code", "t", "lit")

    def __init__(self, code, t, lit=None):
        self.code, self.t, self.lit = code, t, lit


def paren(code):
    """Wrap in parentheses unless the expression is already atomic (a name, literal, or call chain)."""
    depth, in_str, esc = 0, False, False
    for i, ch in enumerate(code):
        if in_str:
            if esc: esc = False
            elif ch == "\\": esc = True
            elif ch == '"': in_str = False
            continue
        if ch == '"': in_str = True
        elif ch in "([": depth += 1
        elif ch in ")]": depth -= 1
        elif depth == 0 and (ch in " +*/%<>=!&|?:," or (ch == "-" and i > 0 and code[i - 1] != ">")):
            return f"({code})"
    return code


def as_type(e, want):
    """Convert an expression to the wanted static type with Scratch's casting rules."""
    t, c = e.t, e.code
    if e.lit is not None:  # literal text: emit directly in the wanted form
        s = e.lit
        n = exact_number(s)
        if want in ("N", "Nq"):
            v = js_to_number(s)
            return cpp_number(0.0 if v is None or v != v else v)
        if want == "S": return cpp_string(s)
        if want == "B": return "true" if js_to_bool(s) else "false"
        return cpp_number(n) if n is not None else cpp_string(s)
    if want == "V":
        return c
    if want in ("N", "Nq"):
        if t == "N": return c
        if t == "Nq": return c if want == "Nq" else f"Nz({c})"
        if t == "V": return f"{paren(c)}.Num()"
        if t == "S": return f"Num({c})"
        if t == "B": return f"Num({c})"
    if want == "S":
        if t == "S": return c
        if t == "V": return f"{paren(c)}.Str()"
        return f"Str({c})"
    if want == "B":
        if t == "B": return c
        if t == "V": return f"{paren(c)}.Bool()"
        return f"Bool({c})"
    raise ValueError((t, want))


def V(e):
    """As a Value (for arguments typed Value); numbers/strings/bools convert implicitly."""
    if e.lit is not None: return as_type(e, "V")
    if e.t == "B": return f"Value({e.code})"
    return e.code


STYLES = {"all around": "AllAround", "left-right": "LeftRight", "don't rotate": "DontRotate"}


class TargetGen:
    def __init__(self, gen, target, index, class_name):
        self.gen, self.t, self.index, self.cls = gen, target, index, class_name
        self.blocks = target["blocks"]
        self.is_stage = target["isStage"]
        self.unsupported = {}

    # ---------- name resolution ----------
    def var_ref(self, var_id, name, is_list=False):
        local = self.gen.local_names[self.index]
        key = ("list" if is_list else "var", var_id)
        if key in local:
            name_ = local[key]
            # a custom block parameter with the same name would hide the member
            return "this->" + name_ if self.current_params and name_ in self.current_params.values() else name_
        stage_names = self.gen.local_names[self.gen.stage_index]
        if key in stage_names:
            return stage_names[key] if self.is_stage else f"stage.{stage_names[key]}"
        # fall back to name lookup (Scratch does this too)
        for k, v in list(local.items()):
            if k[0] == key[0] and self.gen.var_display[(self.index, k)] == name:
                return v
        for k, v in stage_names.items():
            if k[0] == key[0] and self.gen.var_display[(self.gen.stage_index, k)] == name:
                return v if self.is_stage else f"stage.{v}"
        # Missing variable: Scratch would create it; make a stage-level one.
        return self.gen.create_missing(key, name, self.is_stage)

    # ---------- inputs ----------
    def input(self, block, name):
        inp = block.get("inputs", {}).get(name)
        if inp is None:
            return E(None, "S", lit="")
        val = inp[1]
        if val is None:
            return E(None, "S", lit="")
        if isinstance(val, str):
            return self.reporter(val)
        return self.primitive(val)

    def primitive(self, p):
        kind = p[0]
        if kind in (4, 5, 6, 7, 8, 9, 10):
            v = p[1]
            if isinstance(v, (int, float)) and not isinstance(v, bool):
                return E(cpp_number(float(v)), "N")
            return E(None, "S", lit=str(v))
        if kind == 11:
            return E(None, "S", lit=p[1])
        if kind == 12:
            return E(self.var_ref(p[2], p[1]), "V")
        if kind == 13:
            return E(f"{self.var_ref(p[2], p[1], True)}.Contents()", "S")
        return E(None, "S", lit="")

    def field(self, block, name):
        f = block.get("fields", {}).get(name)
        return f[0] if f else ""

    def menu_value(self, block, input_name, field_name):
        """A dropdown input: literal menu value, or a reporter dropped into it."""
        inp = block.get("inputs", {}).get(input_name)
        if inp is None:
            return E(None, "S", lit="")
        val = inp[1]
        if isinstance(val, str) and val in self.blocks:
            b = self.blocks[val]
            if b.get("shadow") and field_name in b.get("fields", {}):
                return E(None, "S", lit=str(b["fields"][field_name][0]))
            return self.reporter(val)
        if isinstance(val, list):
            return self.primitive(val)
        return E(None, "S", lit="")

    # ---------- reporters ----------
    def num(self, b, n): return as_type(self.input(b, n), "N")
    def s(self, b, n): return as_type(self.input(b, n), "S")
    def bool_(self, b, n):
        inp = b.get("inputs", {}).get(n)
        if inp is None or inp[1] is None:
            return "false"
        return as_type(self.input(b, n), "B")

    def literal_number(self, b, name):
        e = self.input(b, name)
        if e.lit is not None:
            v = js_to_number(e.lit)
            return 0.0 if v is None or v != v else v
        if re.fullmatch(r"-?[0-9.]+(e[+-]?[0-9]+)?", e.code or ""):
            return float(e.code)
        return None

    def fold(self, b, op):
        """Both operands are constants: compute now with JavaScript double arithmetic."""
        a, c = self.literal_number(b, "NUM1"), self.literal_number(b, "NUM2")
        if a is None or c is None:
            return None
        try:
            if op == "operator_add": r = a + c
            elif op == "operator_subtract": r = a - c
            elif op == "operator_multiply": r = a * c
            elif op == "operator_divide":
                r = a / c if c != 0 else (math.nan if a == 0 or a != a else math.copysign(math.inf, a) * math.copysign(1, c))
            else:
                if c == 0 or math.isinf(a): r = math.nan
                else:
                    r = math.fmod(a, c)
                    if r / c < 0: r += c
        except OverflowError:
            r = math.inf
        return E(cpp_number(r), "N" if r == r else "Nq")

    def compare(self, b, op):
        a, c = self.input(b, "OPERAND1"), self.input(b, "OPERAND2")
        if a.t in ("N", "Nq") and c.t in ("N", "Nq") and a.lit is None and c.lit is None:
            return E(f"{op}({a.code}, {c.code})", "B")
        # number literal against a typed number: still numeric
        if a.t in ("N", "Nq") and a.lit is None and c.lit is not None and exact_number(c.lit) is not None:
            return E(f"{op}({a.code}, {as_type(c, 'N')})", "B")
        if c.t in ("N", "Nq") and c.lit is None and a.lit is not None and exact_number(a.lit) is not None:
            return E(f"{op}({as_type(a, 'N')}, {c.code})", "B")
        return E(f"{op}({V(a)}, {V(c)})", "B")

    def reporter(self, bid):
        b = self.blocks[bid]
        op = b["opcode"]
        N, S, B = self.num, self.s, self.bool_
        if op in ("math_number", "math_positive_number", "math_whole_number", "math_integer", "math_angle"):
            return E(None, "S", lit=str(self.field(b, "NUM")))
        if op == "text":
            return E(None, "S", lit=str(self.field(b, "TEXT")))
        if op == "colour_picker":
            return E(None, "S", lit=str(self.field(b, "COLOUR")))
        if op in ("operator_add", "operator_subtract", "operator_multiply", "operator_divide", "operator_mod"):
            folded = self.fold(b, op)
            if folded is not None:
                return folded
        if op == "operator_add": return E(f"{N(b,'NUM1')} + {paren(N(b,'NUM2'))}", "N")
        if op == "operator_subtract": return E(f"{N(b,'NUM1')} - {paren(N(b,'NUM2'))}", "N")
        if op == "operator_multiply": return E(f"{paren(N(b,'NUM1'))} * {paren(N(b,'NUM2'))}", "N")
        if op == "operator_divide": return E(f"{paren(N(b,'NUM1'))} / {paren(N(b,'NUM2'))}", "Nq")
        if op == "operator_mod": return E(f"Mod({N(b,'NUM1')}, {N(b,'NUM2')})", "Nq")
        if op == "operator_round": return E(f"Round({N(b,'NUM')})", "N")
        if op == "operator_random": return E(f"PickRandom({V(self.input(b,'FROM'))}, {V(self.input(b,'TO'))})", "N")
        if op == "operator_lt": return self.compare(b, "Lt")
        if op == "operator_gt": return self.compare(b, "Gt")
        if op == "operator_equals": return self.compare(b, "Eq")
        if op == "operator_and": return E(f"{paren(B(b,'OPERAND1'))} && {paren(B(b,'OPERAND2'))}", "B")
        if op == "operator_or": return E(f"{paren(B(b,'OPERAND1'))} || {paren(B(b,'OPERAND2'))}", "B")
        if op == "operator_not": return E(f"!{paren(B(b,'OPERAND'))}", "B")
        if op == "operator_join": return E(f"{S(b,'STRING1')} + {paren(S(b,'STRING2'))}", "S")
        if op == "operator_letter_of": return E(f"LetterOf({N(b,'LETTER')}, {S(b,'STRING')})", "S")
        if op == "operator_length": return E(f"{paren(S(b,'STRING'))}.Length()", "N")
        if op == "operator_contains": return E(f"Contains({S(b,'STRING1')}, {S(b,'STRING2')})", "B")
        if op == "operator_mathop":
            fn = self.field(b, "OPERATOR").lower()
            x = N(b, "NUM")
            table = {"abs": ("std::fabs", "N"), "floor": ("std::floor", "N"), "ceiling": ("std::ceil", "N"),
                     "sqrt": ("std::sqrt", "Nq"), "sin": ("Sin", "Nq"), "cos": ("Cos", "Nq"), "tan": ("Tan", "Nq"),
                     "asin": ("Asin", "Nq"), "acos": ("Acos", "Nq"), "atan": ("Atan", "N"), "ln": ("Ln", "Nq"),
                     "log": ("Log10", "Nq"), "e ^": ("Exp", "N"), "10 ^": ("Pow10", "N")}
            if fn in table:
                f, t = table[fn]
                return E(f"{f}({x})", t)
            return E("0", "N")
        if op == "data_variable":
            f = b["fields"]["VARIABLE"]
            return E(self.var_ref(f[1], f[0]), "V")
        if op == "data_listcontents":
            f = b["fields"]["LIST"]
            return E(f"{self.var_ref(f[1], f[0], True)}.Contents()", "S")
        if op == "data_itemoflist":
            f = b["fields"]["LIST"]
            lst = self.var_ref(f[1], f[0], True)
            idx = self.input(b, "INDEX")
            if idx.t in ("N", "Nq") and idx.lit is None:
                return E(f"{lst}.Item({idx.code})", "V")
            if idx.lit is not None and exact_number(idx.lit) is not None:
                return E(f"{lst}.Item({as_type(idx, 'N')})", "V")
            return E(f"{lst}.Item({V(idx)})", "V")
        if op == "data_lengthoflist":
            f = b["fields"]["LIST"]
            return E(f"{self.var_ref(f[1], f[0], True)}.Length()", "N")
        if op == "data_itemnumoflist":
            f = b["fields"]["LIST"]
            return E(f"{self.var_ref(f[1], f[0], True)}.IndexOf({V(self.input(b,'ITEM'))})", "N")
        if op == "data_listcontainsitem":
            f = b["fields"]["LIST"]
            return E(f"{self.var_ref(f[1], f[0], True)}.Contains({V(self.input(b,'ITEM'))})", "B")
        if op in ("argument_reporter_string_number", "argument_reporter_boolean"):
            name = self.field(b, "VALUE")
            params = self.current_params or {}
            if name in params:
                return E(params[name], "V")
            return E("0", "N")
        if op == "motion_xposition": return E("XPosition()", "N")
        if op == "motion_yposition": return E("YPosition()", "N")
        if op == "motion_direction": return E("Direction", "N")
        if op == "looks_size": return E("SizeReporter()", "N")
        if op == "looks_costumenumbername":
            return E("CostumeName()", "S") if self.field(b, "NUMBER_NAME") == "name" else E("CostumeNumber()", "N")
        if op == "looks_backdropnumbername":
            st = "" if self.is_stage else "stage."
            return E(f"{st}CostumeName()", "S") if self.field(b, "NUMBER_NAME") == "name" else E(f"{st}CostumeNumber()", "N")
        if op == "sensing_mousex": return E("MouseX()", "N")
        if op == "sensing_mousey": return E("MouseY()", "N")
        if op == "sensing_mousedown": return E("MouseDown()", "B")
        if op == "sensing_timer": return E("Timer()", "N")
        if op == "sensing_keypressed":
            return E(f"KeyPressed({V(self.menu_value(b, 'KEY_OPTION', 'KEY_OPTION'))})", "B")
        if op == "sensing_touchingobject":
            m = self.menu_value(b, "TOUCHINGOBJECTMENU", "TOUCHINGOBJECTMENU")
            if m.lit == "_mouse_" and not self.is_stage:
                return E("TouchingMouse()", "B")
            self.note(op + " (" + str(m.lit) + ")")
            return E("false", "B")
        if op == "sound_volume": return E("Volume", "N")
        self.note(op)
        return E("Value()", "V")

    def note(self, op):
        self.unsupported[op] = self.unsupported.get(op, 0) + 1

    # ---------- statements ----------
    def stack(self, bid, ctx, ind):
        out = []
        while bid:
            b = self.blocks[bid]
            out.extend(self.statement(b, ctx, ind))
            bid = b.get("next")
        return out

    def substack(self, b, name, ctx, ind):
        inp = b.get("inputs", {}).get(name)
        if not inp or not inp[1]:
            return []
        return self.stack(inp[1], ctx, ind)

    def loop_yield(self, ctx, ind):
        return [ind + "co_yield NextFrame; // next frame (Scratch loops pause here unless run without screen refresh)"] \
            if ctx["iter"] and not ctx["warp"] else []

    def lone_if(self, b, name):
        """The single "if" block inside an else branch (emitted as `else if`), or None."""
        inp = b.get("inputs", {}).get(name)
        if not inp or not inp[1]:
            return None
        inner = self.blocks[inp[1]]
        if inner["opcode"] in ("control_if", "control_if_else") and not inner.get("next"):
            return inner
        return None

    def if_chain(self, b, ctx, ind, prefix="if"):
        Bo = self.bool_
        I = ind + "    "
        out = [f"{ind}{prefix} ({Bo(b,'CONDITION')})", ind + "{", *self.substack(b, "SUBSTACK", ctx, I), ind + "}"]
        if b["opcode"] == "control_if_else":
            inner = self.lone_if(b, "SUBSTACK2")
            if inner is not None:
                out += self.if_chain(inner, ctx, ind, "else if")
            else:
                out += [ind + "else", ind + "{", *self.substack(b, "SUBSTACK2", ctx, I), ind + "}"]
        return out

    def statement(self, b, ctx, ind):
        op = b["opcode"]
        N, S, Bo = self.num, self.s, self.bool_
        I = ind + "    "
        if op in ("control_if", "control_if_else"):
            return self.if_chain(b, ctx, ind)
        if op == "control_repeat":
            d = ctx["depth"] = ctx.get("depth", 0) + 1
            iv, nv = f"i{d}", f"times{d}"
            body = self.substack(b, "SUBSTACK", ctx, I)
            ctx["depth"] -= 1
            return [f"{ind}for (double {iv} = 0, {nv} = Round({N(b,'TIMES')}); {iv} < {nv}; {iv}++)", ind + "{",
                    *body, *self.loop_yield(ctx, I), ind + "}"]
        if op == "control_repeat_until":
            return [f"{ind}while (!{paren(Bo(b,'CONDITION'))})", ind + "{", *self.substack(b, "SUBSTACK", ctx, I),
                    *self.loop_yield(ctx, I), ind + "}"]
        if op == "control_while":
            return [f"{ind}while ({Bo(b,'CONDITION')})", ind + "{", *self.substack(b, "SUBSTACK", ctx, I),
                    *self.loop_yield(ctx, I), ind + "}"]
        if op == "control_forever":
            return [f"{ind}while (true)", ind + "{", *self.substack(b, "SUBSTACK", ctx, I), *self.loop_yield(ctx, I), ind + "}"]
        if op == "control_wait":
            if ctx["iter"] and not ctx["warp"]:
                return [f"{ind}co_await Wait({N(b,'DURATION')});"]
            return [f"{ind}BusyWait({N(b,'DURATION')});"]
        if op == "control_wait_until":
            if ctx["iter"] and not ctx["warp"]:
                return [f"{ind}while (!{paren(Bo(b,'CONDITION'))}) co_yield NextFrame;"]
            return [f"{ind}while (!{paren(Bo(b,'CONDITION'))}) {{ }}"]
        if op == "control_stop":
            what = self.field(b, "STOP_OPTION")
            if what == "this script":
                return [ind + ("co_return;" if ctx["iter"] else "return;")]
            if what == "all":
                return [ind + "StopAll();"]
            return [ind + "StopOtherScriptsInSprite();"]
        if op == "control_create_clone_of":
            m = self.menu_value(b, "CLONE_OPTION", "CLONE_OPTION")
            if m.lit == "_myself_" and not self.is_stage:
                return [ind + "CreateClone();"]
            return [f"{ind}GetEngine().CreateCloneOf({as_type(m, 'S')});"]
        if op == "control_delete_this_clone":
            return [ind + "DeleteThisClone();"] if not self.is_stage else []
        if op == "event_broadcast":
            return [f"{ind}Broadcast({S(b,'BROADCAST_INPUT')});"]
        if op == "data_setvariableto":
            f = b["fields"]["VARIABLE"]
            return [f"{ind}{self.var_ref(f[1], f[0])} = {V(self.input(b,'VALUE'))};"]
        if op == "data_changevariableby":
            f = b["fields"]["VARIABLE"]
            v = self.var_ref(f[1], f[0])
            return [f"{ind}{v} = {v}.Num() + {paren(N(b,'VALUE'))};"]
        if op in ("data_addtolist", "data_deleteoflist", "data_deletealloflist", "data_insertatlist",
                  "data_replaceitemoflist", "data_showlist", "data_hidelist"):
            f = b["fields"]["LIST"]
            lst = self.var_ref(f[1], f[0], True)
            if op == "data_addtolist": return [f"{ind}{lst}.Add({V(self.input(b,'ITEM'))});"]
            if op == "data_deleteoflist": return [f"{ind}{lst}.Delete({V(self.input(b,'INDEX'))});"]
            if op == "data_deletealloflist": return [f"{ind}{lst}.DeleteAll();"]
            if op == "data_insertatlist": return [f"{ind}{lst}.Insert({V(self.input(b,'INDEX'))}, {V(self.input(b,'ITEM'))});"]
            if op == "data_replaceitemoflist": return [f"{ind}{lst}.Replace({V(self.input(b,'INDEX'))}, {V(self.input(b,'ITEM'))});"]
            if op == "data_showlist": return [f"{ind}ShowList({lst});"]
            return [f"{ind}HideList({lst});"]
        if op == "procedures_call":
            return self.call(b, ctx, ind)
        # motion
        if op == "motion_gotoxy": return [f"{ind}GoTo({N(b,'X')}, {N(b,'Y')});"]
        if op == "motion_setx": return [f"{ind}SetX({N(b,'X')});"]
        if op == "motion_sety": return [f"{ind}SetY({N(b,'Y')});"]
        if op == "motion_changexby": return [f"{ind}ChangeX({N(b,'DX')});"]
        if op == "motion_changeyby": return [f"{ind}ChangeY({N(b,'DY')});"]
        if op == "motion_pointindirection": return [f"{ind}PointInDirection({N(b,'DIRECTION')});"]
        if op == "motion_turnright": return [f"{ind}TurnRight({N(b,'DEGREES')});"]
        if op == "motion_turnleft": return [f"{ind}TurnLeft({N(b,'DEGREES')});"]
        if op == "motion_setrotationstyle":
            style = STYLES.get(self.field(b, "STYLE"), "AllAround")
            return [f"{ind}Rotation = RotationStyle::{style};"]
        # looks
        if op in ("looks_switchcostumeto", "looks_switchbackdropto"):
            key = "COSTUME" if op == "looks_switchcostumeto" else "BACKDROP"
            target = "" if op == "looks_switchcostumeto" or self.is_stage else "stage."
            return [f"{ind}{target}SwitchCostume({V(self.menu_value(b, key, key))});"]
        if op == "looks_nextcostume": return [f"{ind}SetCostumeIndex(CurrentCostume + 1);"]
        if op in ("looks_seteffectto", "looks_changeeffectby"):
            eff = self.field(b, "EFFECT").capitalize()
            fn = "SetEffect" if op == "looks_seteffectto" else "ChangeEffect"
            return [f"{ind}{fn}(Effect::{eff}, {N(b,'VALUE' if op == 'looks_seteffectto' else 'CHANGE')});"]
        if op == "looks_cleargraphiceffects": return [ind + "ClearEffects();"]
        if op == "looks_setsizeto": return [f"{ind}SetSize({N(b,'SIZE')});"]
        if op == "looks_changesizeby": return [f"{ind}ChangeSize({N(b,'CHANGE')});"]
        if op == "looks_show": return [ind + "Show();"]
        if op == "looks_hide": return [ind + "Hide();"]
        # pen
        if op == "pen_clear": return [ind + "PenClear();"]
        if op == "pen_stamp": return [ind + "Stamp();"]
        if op == "pen_penDown": return [ind + "PenDown();"]
        if op == "pen_penUp": return [ind + "PenUp();"]
        if op == "pen_setPenColorToColor": return [f"{ind}SetPenColor({V(self.input(b,'COLOR'))});"]
        if op in ("pen_setPenColorParamTo", "pen_changePenColorParamBy"):
            m = self.menu_value(b, "COLOR_PARAM", "colorParam")
            fn = "SetPenParam" if op == "pen_setPenColorParamTo" else "ChangePenParam"
            if m.lit is not None and m.lit in ("color", "saturation", "brightness", "transparency"):
                return [f"{ind}{fn}(PenParam::{m.lit.capitalize()}, {N(b,'VALUE')});"]
            return [f"{ind}{fn}(ParsePenParam({as_type(m,'S')}), {N(b,'VALUE')});"]
        if op == "pen_setPenSizeTo": return [f"{ind}SetPenSize({N(b,'SIZE')});"]
        if op == "pen_changePenSizeBy": return [f"{ind}ChangePenSize({N(b,'SIZE')});"]
        # sound
        if op == "sound_play": return [f"{ind}StartSound({V(self.menu_value(b,'SOUND_MENU','SOUND_MENU'))});"]
        if op == "sound_stopallsounds": return [ind + "StopAllSounds();"]
        if op in ("sound_seteffectto", "sound_changeeffectby"):
            eff = self.field(b, "EFFECT").capitalize()
            fn = "SetSoundEffect" if op == "sound_seteffectto" else "ChangeSoundEffect"
            return [f"{ind}{fn}(SoundEffect::{eff}, {N(b,'VALUE')});"]
        if op == "sound_cleareffects": return [ind + "ClearSoundEffects();"]
        if op == "sound_setvolumeto": return [f"{ind}SetVolume({N(b,'VOLUME')});"]
        if op == "sound_changevolumeby": return [f"{ind}ChangeVolume({N(b,'VOLUME')});"]
        # sensing
        if op == "sensing_resettimer": return [ind + "ResetTimer();"]
        self.note(op)
        return [f"{ind}// (unsupported block: {op})"]

    def call(self, b, ctx, ind):
        m = b["mutation"]
        proc = self.procs.get(m["proccode"])
        if not proc:
            return [f"{ind}// call to undefined block: {m['proccode']}"]
        arg_ids = json.loads(m["argumentids"])
        kinds = re.findall(r"%([snb])", proc["proccode"])
        args = []
        for i, pid in enumerate(proc["ids"]):
            if pid in arg_ids and pid in b.get("inputs", {}) and b["inputs"][pid][1] is not None:
                args.append(V(self.input(b, pid)))
            elif i < len(kinds) and kinds[i] == "b":
                args.append("false")  # empty boolean slot
            else:
                d = proc["defaults"][i] if i < len(proc["defaults"]) else ""
                args.append(as_type(E(None, "S", lit=str(d)), "V"))
        call = f"{proc['method']}({', '.join(args)})"
        if proc["iter"]:
            if ctx["iter"] and not ctx["warp"]:
                return [f"{ind}co_await {call};"]
            return [f"{ind}{proc['method']}_Warp({', '.join(args)});"]
        return [f"{ind}{call};"]

    # ---------- procedures ----------
    def collect_procs(self, names):
        self.procs = {}
        for bid, b in self.blocks.items():
            if not isinstance(b, dict) or b["opcode"] != "procedures_definition":
                continue
            proto = self.blocks[b["inputs"]["custom_block"][1]]
            m = proto["mutation"]
            code = m["proccode"]
            if code in self.procs:
                continue
            arg_names = json.loads(m["argumentnames"])
            pnames = Names()
            params = [pnames.get(camel(a, "arg")) for a in arg_names]
            label = re.sub(r"%[snb]", " ", code)
            self.procs[code] = {
                "def": bid, "proccode": code, "names": arg_names, "params": params,
                "ids": json.loads(m["argumentids"]),
                "defaults": json.loads(m.get("argumentdefaults", "[]") or "[]"),
                "warp": m.get("warp") in (True, "true"),
                "method": names.get(pascal(label, "CustomBlock")),
                "iter": False,
            }
        # A procedure needs to be a coroutine if it can pause: not "run without screen refresh"
        # and it has a loop/wait or calls a procedure that can pause.
        pausing = {"control_repeat", "control_repeat_until", "control_forever", "control_wait", "control_wait_until", "control_while"}
        changed = True
        while changed:
            changed = False
            for p in self.procs.values():
                if p["warp"] or p["iter"]:
                    continue
                for ob in self.blocks_in(p["def"]):
                    if ob["opcode"] in pausing or (ob["opcode"] == "procedures_call"
                                                     and self.procs.get(ob["mutation"]["proccode"], {}).get("iter")):
                        p["iter"] = True; changed = True
                        break

    def blocks_in(self, top):
        todo, seen = [top], set()
        while todo:
            bid = todo.pop()
            if bid in seen or bid not in self.blocks:
                continue
            seen.add(bid)
            b = self.blocks[bid]
            if not isinstance(b, dict):
                continue
            yield b
            if b.get("next"): todo.append(b["next"])
            for inp in b.get("inputs", {}).values():
                for v in inp[1:]:
                    if isinstance(v, str): todo.append(v)

    def proc_label(self, p):
        label = p["proccode"]
        for nm in p["names"]:
            label = re.sub(r"%[snb]", lambda _m: "(" + nm + ")", label, count=1)
        return label

    def proc_method(self, p, warp_variant=False):
        """(declaration lines for the header, definition lines for the .cpp)"""
        b = self.blocks[p["def"]]
        self.current_params = dict(zip(p["names"], p["params"]))
        sig = ", ".join(f"Value {n}" for n in p["params"])
        label = self.proc_label(p)
        if p["iter"] and not warp_variant:
            ctx = {"iter": True, "warp": False}
            ret, name = "Script", p["method"]
            doc = f"    /// Custom block `{comment_safe(label)}`."
        else:
            ctx = {"iter": False, "warp": bool(p["warp"] or warp_variant)}
            ret, name = "void", p["method"] + ("_Warp" if warp_variant else "")
            doc = f"    /// Custom block `{comment_safe(label)}`" + (" (run without screen refresh)." if p["warp"] else ".")
            if warp_variant:
                doc = f"    /// Same as {p['method']}, called from code that runs without screen refresh."
        body = self.stack(b.get("next"), ctx, "    ")
        if ret == "Script" and not any(("co_yield" in line or "co_await" in line or "co_return" in line) for line in body):
            body.append("    co_return;")
        self.current_params = None
        decl = [doc, f"    {ret} {name}({sig});"]
        defn = [f"// {comment_safe(label)}" + (" (run without screen refresh)" if p["warp"] else ""),
                f"{ret} {self.cls}::{name}({sig})", "{", *body, "}", ""]
        return decl, defn

    # ---------- hats ----------
    def hats(self, names):
        out = []
        for bid, b in self.blocks.items():
            if not isinstance(b, dict) or not b.get("topLevel"):
                continue
            op = b["opcode"]
            if op == "event_whenflagclicked":
                out.append(("GreenFlag", "", names.get("WhenGreenFlagClicked"), bid, "when green flag clicked"))
            elif op == "event_whenbroadcastreceived":
                name = self.field(b, "BROADCAST_OPTION")
                out.append(("Broadcast", name, names.get("WhenIReceive" + pascal(name, "Message")), bid, f"when I receive [{name}]"))
            elif op == "event_whenkeypressed":
                key = self.field(b, "KEY_OPTION")
                out.append(("KeyPressed", key, names.get("WhenKey" + pascal(key, "Key") + "Pressed"), bid, f"when [{key}] key pressed"))
            elif op == "control_start_as_clone":
                out.append(("CloneStart", "", names.get("WhenIStartAsAClone"), bid, "when I start as a clone"))
            elif op.startswith("event_") or op.startswith("control_start"):
                self.note(op)
        return out

    def generate(self, ns, members_include=None):
        """Returns (header text, list of .cpp texts)."""
        names = Names()
        self.current_params = None
        self.collect_procs(names)
        hats = self.hats(names)
        local = self.gen.local_names[self.index]
        kind = "stage" if self.is_stage else "sprite " + json.dumps(self.t["name"])
        guard = f"{self.cls.upper()}_G_H"

        H = ["// <auto-generated>",
             f"//   Generated from the Scratch {kind} by sb3_to_cpp.py.",
             "//   Each Scratch custom block is a method; each event script (\"when ...\") is a coroutine that",
             "//   the engine resumes once per frame wherever Scratch would wait for the next frame.",
             "// </auto-generated>",
             "#pragma once",
             "#include <string>",
             "#include <vector>",
             "",
             '#include "Target.h"']
        if not self.is_stage:
            H.append('#include "Stage.g.h"')
        if members_include and members_include.endswith(".members.h"):
            # headers the hand-written members need (they can't be included inside the class body)
            H.append(f'#include "{members_include[:-len(".members.h")]}.includes.h"')
        H += ["", f"namespace {ns} {{", "", "using namespace scratch;", "using namespace scratch::ops;", ""]
        base = "StageBase" if self.is_stage else "SpriteBase"
        H += [f"class {self.cls} : public {base}", "{", "public:"]
        if not self.is_stage:
            H += ["    Stage& stage;", ""]
        vars_ = [(k, v) for k, v in local.items() if k[0] == "var"]
        lists = [(k, v) for k, v in local.items() if k[0] == "list"]
        if vars_:
            H.append("    // ---- variables" + (" (global)" if self.is_stage else " (this sprite only)") + " ----")
            for k, v in vars_:
                init = self.gen.var_init[(self.index, k)]
                disp = self.gen.var_display[(self.index, k)]
                H.append(f"    Value {v} = {init};" + (f" // {comment_safe(disp)}" if camel(disp) != v else ""))
            H.append("")
        if lists:
            H.append("    // ---- lists (starting contents are loaded from assets/initial-lists.json) ----")
            for k, v in lists:
                disp = self.gen.var_display[(self.index, k)]
                H.append(f"    ScratchList {v}{{{cpp_string(disp)}}};")
            H.append("")
        if self.is_stage:
            H += [f"    {self.cls}() = default;", ""]
            H += ["    /// Every variable and list by name (used to save and load projects).",
                  f"    static const std::vector<std::pair<const char*, Value {self.cls}::*>>& ValueFields();",
                  f"    static const std::vector<std::pair<const char*, ScratchList {self.cls}::*>>& ListFields();", ""]
        else:
            H += [f"    explicit {self.cls}(Stage& stage) : stage(stage) {{ BaseStage = &stage; }}", ""]
        H.append("    const std::vector<Hat>& Hats() const override;")
        if not self.is_stage:
            H.append(f"    SpriteBase* CloneSelf() const override {{ return new {self.cls}(*this); }}")
        H.append("")

        defs = []  # (lines) per method, so they can be split over several .cpp files
        if hats:
            H.append("    // =================== event scripts ===================")
        for kind_, key, method, bid, label in hats:
            H.append(f"    /// Scratch: `{comment_safe(label)}`")
            H.append(f"    Script {method}();")
            body = self.stack(self.blocks[bid].get("next"), {"iter": True, "warp": False}, "    ")
            if not any(("co_yield" in line or "co_await" in line or "co_return" in line) for line in body):
                body.append("    co_return;")
            defs.append([f"// Scratch: {comment_safe(label)}", f"Script {self.cls}::{method}()", "{", *body, "}", ""])
        if hats:
            H.append("")
        if self.procs:
            H.append("    // =================== custom blocks ===================")
        for p in self.procs.values():
            decl, defn = self.proc_method(p)
            H += decl
            defs.append(defn)
            if p["iter"]:
                decl, defn = self.proc_method(p, warp_variant=True)
                H += decl
                defs.append(defn)
        if members_include:
            H += ["", "    // ---- hand-written members (not generated; see the file below) ----",
                  f'#include "{members_include}"']
        H += ["};", "", f"}} // namespace {ns}", ""]

        # Hat table (and the stage's field tables) go in the first .cpp file.
        first = []
        first.append(f"const std::vector<Hat>& {self.cls}::Hats() const")
        first.append("{")
        first.append("    static const std::vector<Hat> hats = {")
        for kind_, key, method, _, _ in hats:
            first.append(f"        {{HatKind::{kind_}, {cpp_string(key)}, [](Target* t) {{ return static_cast<{self.cls}*>(t)->{method}(); }}}},")
        first.append("    };")
        first.append("    return hats;")
        first.append("}")
        first.append("")
        if self.is_stage:
            first.append(f"const std::vector<std::pair<const char*, Value {self.cls}::*>>& {self.cls}::ValueFields()")
            first.append("{")
            first.append(f"    static const std::vector<std::pair<const char*, Value {self.cls}::*>> fields = {{")
            for k, v in vars_:
                first.append(f'        {{"{v}", &{self.cls}::{v}}},')
            first.append("    };")
            first.append("    return fields;")
            first.append("}")
            first.append("")
            first.append(f"const std::vector<std::pair<const char*, ScratchList {self.cls}::*>>& {self.cls}::ListFields()")
            first.append("{")
            first.append(f"    static const std::vector<std::pair<const char*, ScratchList {self.cls}::*>> fields = {{")
            for k, v in lists:
                first.append(f'        {{"{v}", &{self.cls}::{v}}},')
            first.append("    };")
            first.append("    return fields;")
            first.append("}")
            first.append("")
        defs.insert(0, first)

        # Split the definitions into .cpp files of about MAX_LINES lines (faster parallel builds).
        MAX_LINES = 3500
        chunks, cur, n = [], [], 0
        for d in defs:
            if cur and n + len(d) > MAX_LINES:
                chunks.append(cur); cur, n = [], 0
            cur.append(d); n += len(d)
        if cur: chunks.append(cur)
        cpps = []
        for i, chunk in enumerate(chunks):
            C = ["// <auto-generated> Generated by sb3_to_cpp.py" +
                 (f" (part {i + 1} of {len(chunks)})" if len(chunks) > 1 else "") + ". </auto-generated>",
                 f'#include "{self.cls}.g.h"', "", "#include <cmath>", "",
                 f"namespace {ns} {{", ""]
            for d in chunk:
                C.extend(d)
            C += [f"}} // namespace {ns}", ""]
            cpps.append("\n".join(C))
        return "\n".join(H), cpps


def comment_safe(s):
    return s.replace("*/", "* /").replace("\n", " ").replace("\\", "/")


class Generator:
    def __init__(self, project):
        self.p = project
        self.targets = project["targets"]
        self.stage_index = next(i for i, t in enumerate(self.targets) if t["isStage"])
        self.local_names = {}
        self.var_init = {}
        self.var_display = {}
        self.list_init = {}
        for i, t in enumerate(self.targets):
            names = Names({"stage"})
            local = {}
            for vid, v in t.get("variables", {}).items():
                key = ("var", vid)
                local[key] = names.get(camel(v[0], "variable"))
                self.var_display[(i, key)] = v[0]
                self.var_init[(i, key)] = self.literal(v[1])
            for lid, l in t.get("lists", {}).items():
                key = ("list", lid)
                local[key] = names.get(camel(l[0], "list"))
                self.var_display[(i, key)] = l[0]
                self.list_init[(i, key)] = l[1]
            self.local_names[i] = local

    def literal(self, v):
        if isinstance(v, bool): return "true" if v else "false"
        if isinstance(v, (int, float)): return cpp_number(float(v))
        n = exact_number(str(v))
        return cpp_number(n) if n is not None else cpp_string(str(v))

    def create_missing(self, key, name, is_stage):
        si = self.stage_index
        stage_local = self.local_names[si]
        if key not in stage_local:
            n = Names(set(stage_local.values())).get(camel(name, "variable"))
            stage_local[key] = n
            self.var_display[(si, key)] = name
            if key[0] == "var": self.var_init[(si, key)] = 'u""_s'
            else: self.list_init[(si, key)] = []
        return stage_local[key] if is_stage else f"stage.{stage_local[key]}"


def main():
    args = sys.argv[1:]
    src, out_dir, assets_dir = args[0], args[1], args[2]
    ns = args[args.index("--namespace") + 1] if "--namespace" in args else "Game"
    title = args[args.index("--title") + 1] if "--title" in args else "Scratch Project"
    project = json.load(open(os.path.join(src, "project.json"), encoding="utf-8"))
    # --rename "old=New": friendlier class name for a sprite (Scratch names can be anything)
    renames = dict(a.split("=", 1) for i, a in enumerate(args) if i > 0 and args[i - 1] == "--rename")
    members = dict(a.split("=", 1) for i, a in enumerate(args) if i > 0 and args[i - 1] == "--members")
    gen = Generator(project)
    os.makedirs(out_dir, exist_ok=True)
    for f in os.listdir(out_dir):
        if f.endswith(".g.h") or f.endswith(".g.cpp"):
            os.remove(os.path.join(out_dir, f))

    class_names = Names({"Stage", "Project", "Program"})
    classes = {}
    for i, t in enumerate(gen.targets):
        if t["isStage"]:
            classes[i] = "Stage"
        else:
            base = pascal(renames.get(t["name"], t["name"]), "")
            classes[i] = class_names.get((base if base and not base.startswith("_") else "Sprite" + str(i)) + "Sprite")

    # Generate the stage last so variables created on the fly by sprites are included.
    order = [i for i in range(len(gen.targets)) if i != gen.stage_index] + [gen.stage_index]
    unsupported = {}
    files = {}
    for i in order:
        tg = TargetGen(gen, gen.targets[i], i, classes[i])
        files[i] = tg.generate(ns, members.get(classes[i]))
        for k, v in tg.unsupported.items():
            unsupported[k] = unsupported.get(k, 0) + v
    cpp_files = []
    for i in range(len(gen.targets)):
        header, cpps = files[i]
        with open(os.path.join(out_dir, classes[i] + ".g.h"), "w", encoding="utf-8", newline="\n") as f:
            f.write(header)
        for k, text in enumerate(cpps):
            name = classes[i] + (f".{k + 1}" if len(cpps) > 1 else "") + ".g.cpp"
            cpp_files.append(name)
            with open(os.path.join(out_dir, name), "w", encoding="utf-8", newline="\n") as f:
                f.write(text)

    # Initial list contents -> JSON (keyed by "<target index>/<field name>")
    lists_out = {}
    for (i, key), items in gen.list_init.items():
        lists_out[f"{i}/{gen.local_names[i][key]}"] = [x if isinstance(x, (int, float, bool)) else str(x) for x in items]
    with open(os.path.join(assets_dir, "initial-lists.json"), "w", encoding="utf-8") as f:
        json.dump(lists_out, f, ensure_ascii=False, separators=(",", ":"))

    # Project.g.h / Project.g.cpp: creates the targets in their starting state.
    sprite_headers = "".join(f'#include "{classes[i]}.g.h"\n' for i in range(len(gen.targets)) if i != gen.stage_index)
    PH = ["// <auto-generated> Creates the stage and sprites with their starting state. </auto-generated>",
          "#pragma once", "#include <string>", "", '#include "Engine.h"', "",
          f"namespace {ns}::Project {{", "",
          f"inline constexpr const char* Title = {json.dumps(title)};", "",
          "void Create(scratch::Engine& engine, const scratch::AssetManifest& assets, const std::string& assetDir,",
          "            const scratch::InitialLists& lists);", "",
          f"}} // namespace {ns}::Project", ""]
    with open(os.path.join(out_dir, "Project.g.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(PH))

    P = ["// <auto-generated> Creates the stage and sprites with their starting state. </auto-generated>",
         '#include "Project.g.h"', "", '#include "Stage.g.h"', sprite_headers.rstrip("\n"), "",
         f"namespace {ns}::Project {{", "", "using namespace scratch;", "",
         "void Create(Engine& engine, const AssetManifest& assets, const std::string& assetDir, const InitialLists& lists)",
         "{"]
    st = gen.targets[gen.stage_index]
    P.append("    auto* stage = new Stage();")
    P.append(f"    std::tie(stage->Costumes, stage->Sounds) = assets.For({gen.stage_index}, assetDir);")
    P.append(f"    stage->CurrentCostume = {st.get('currentCostume', 0)};")
    P.append(f"    stage->Volume = {cpp_number(float(st.get('volume', 100)))};")
    for key, v in gen.local_names[gen.stage_index].items():
        if key[0] == "list":
            P.append(f"    lists.Fill(stage->{v}, \"{gen.stage_index}/{v}\");")
    P.append("    engine.SetStage(stage);")
    sprites = sorted([i for i in range(len(gen.targets)) if i != gen.stage_index], key=lambda i: gen.targets[i].get("layerOrder", i))
    for i in sprites:
        t = gen.targets[i]
        var = "sprite" + str(i)
        style = STYLES.get(t.get("rotationStyle"), "AllAround")
        P.append("")
        P.append(f"    auto* {var} = new {classes[i]}(*stage);")
        P.append(f"    {var}->Name = {cpp_string(t['name'])};")
        P.append(f"    {var}->X = {cpp_number(float(t.get('x', 0)))};")
        P.append(f"    {var}->Y = {cpp_number(float(t.get('y', 0)))};")
        P.append(f"    {var}->Direction = {cpp_number(float(t.get('direction', 90)))};")
        P.append(f"    {var}->Size = {cpp_number(float(t.get('size', 100)))};")
        P.append(f"    {var}->Visible = {'true' if t.get('visible', True) else 'false'};")
        P.append(f"    {var}->Rotation = RotationStyle::{style};")
        P.append(f"    {var}->CurrentCostume = {t.get('currentCostume', 0)};")
        P.append(f"    {var}->Volume = {cpp_number(float(t.get('volume', 100)))};")
        P.append(f"    std::tie({var}->Costumes, {var}->Sounds) = assets.For({i}, assetDir);")
        for key, v in gen.local_names[i].items():
            if key[0] == "list":
                P.append(f"    lists.Fill({var}->{v}, \"{i}/{v}\");")
        P.append(f"    engine.AddSprite({var});")
    # list monitors
    list_fields = {}
    for (i, key), name in gen.var_display.items():
        if key[0] == "list":
            list_fields[key[1]] = (i, gen.local_names[i][key])
    for m in project.get("monitors", []):
        if m.get("mode") != "list":
            continue
        lid = m.get("id")
        if lid not in list_fields:
            continue
        i, field = list_fields[lid]
        owner = "stage" if i == gen.stage_index else "sprite" + str(i)
        P.append(f"    engine.monitors.AddList({owner}->{field}, {m.get('x', 0)}, {m.get('y', 0)}, {m.get('width', 0)}, {m.get('height', 0)}, {'true' if m.get('visible') else 'false'});")
    P += ["}", "", f"}} // namespace {ns}::Project", ""]
    with open(os.path.join(out_dir, "Project.g.cpp"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(P))

    total = sum(1 for t in gen.targets for b in t["blocks"].values() if isinstance(b, dict))
    print(f"generated {len(gen.targets)} targets, {total} blocks, {len(cpp_files) + 1} .cpp files")
    if unsupported:
        print("unsupported blocks (left as comments):", unsupported)


if __name__ == "__main__":
    main()
