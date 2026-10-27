// Scratch's operator blocks. Generated code calls these directly (using namespace scratch::ops).
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>

#include "Value.h"

namespace scratch {

inline constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
inline constexpr double kInfinity = std::numeric_limits<double>::infinity();
inline constexpr double kPi = 3.14159265358979323846;

namespace ops {

// ---- conversions used by generated code ----
inline double Num(const String& s) { return Js::ToNumberOrZero(s); }
inline double Num(bool b) { return b ? 1 : 0; }
inline String Str(double n) { return Js::NumberToString(n); }
inline String Str(int n) { return Js::NumberToString(n); }
inline String Str(bool b) { return b ? u"true"_s : u"false"_s; }
inline bool Bool(double n) { return n != 0 && n == n; }
inline bool Bool(int n) { return n != 0; }
inline bool Bool(const String& s) { return Js::StringToBool(s); }
/// Scratch turns NaN into 0 wherever a block reads a number.
inline double Nz(double n) { return n != n ? 0 : n; }

// ---- comparison (Scratch's Cast.compare) ----
double Compare(const Value& a, const Value& b);

// Fast paths when both sides are known numbers (NaN still follows Scratch's text rules).
inline bool Eq(double a, double b) { return a == b || ((a != a || b != b) && Compare(a, b) == 0); }
inline bool Lt(double a, double b) { return (a != a || b != b) ? Compare(a, b) < 0 : a < b; }
inline bool Gt(double a, double b) { return (a != a || b != b) ? Compare(a, b) > 0 : a > b; }

inline bool Eq(const Value& a, const Value& b)
{
    if (a.IsNumber() && b.IsNumber()) return Eq(a.RawNumber(), b.RawNumber());
    return Compare(a, b) == 0;
}
inline bool Lt(const Value& a, const Value& b)
{
    return (a.IsNumber() && b.IsNumber()) ? Lt(a.RawNumber(), b.RawNumber()) : Compare(a, b) < 0;
}
inline bool Gt(const Value& a, const Value& b)
{
    return (a.IsNumber() && b.IsNumber()) ? Gt(a.RawNumber(), b.RawNumber()) : Compare(a, b) > 0;
}

// ---- math ----
inline double Mod(double n, double m)
{
    double r = std::fmod(n, m);
    return r / m < 0 ? r + m : r;
}

/// JavaScript Math.round (halves round up).
inline double Round(double x)
{
    if (x != x || std::isinf(x)) return x;
    double f = std::floor(x);
    return x - f >= 0.5 ? f + 1 : f;
}

inline double Sin(double deg) { return Round(std::sin(kPi * deg / 180) * 1e10) / 1e10; }
inline double Cos(double deg) { return Round(std::cos(kPi * deg / 180) * 1e10) / 1e10; }
double Tan(double deg);
inline double Asin(double n) { return std::asin(n) * 180 / kPi; }
inline double Acos(double n) { return std::acos(n) * 180 / kPi; }
inline double Atan(double n) { return std::atan(n) * 180 / kPi; }
inline double Ln(double n) { return std::log(n); }
inline double Log10(double n) { return std::log(n) / std::log(10.0); }
inline double Exp(double n) { return std::exp(n); }
inline double Pow10(double n) { return std::pow(10.0, n); }

double PickRandom(const Value& from, const Value& to);
double RandomUnit();

// ---- text ----
String LetterOf(double letter, const String& s);
bool Contains(const String& a, const String& b);

// ---- JavaScript's Math.min / Math.max: NaN in, NaN out (std::min/std::max would quietly pick a side) ----
inline double JsMin(double a, double b) { return (a != a || b != b) ? kNaN : (a < b ? a : b); }
inline double JsMax(double a, double b) { return (a != a || b != b) ? kNaN : (a > b ? a : b); }

// ---- Scratch's "clamp" and "wrapClamp" ----
/// Scratch's MathUtil.clamp. A NaN stays NaN, as in Scratch: a NaN effect is then simply not applied.
inline double Clamp(double n, double min, double max) { return JsMax(min, JsMin(n, max)); }
inline double WrapClamp(double n, double min, double max)
{
    double range = max - min + 1;
    return n - std::floor((n - min) / range) * range;
}

} // namespace ops
} // namespace scratch
