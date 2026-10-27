#include "Ops.h"

#include <random>

namespace scratch::ops {

double Compare(const Value& a, const Value& b)
{
    double n1 = a.JsNumber(), n2 = b.JsNumber();
    if (n1 == 0 && a.IsWhiteSpace()) n1 = kNaN;
    else if (n2 == 0 && b.IsWhiteSpace()) n2 = kNaN;
    if (n1 != n1 || n2 != n2)
    {
        String s1 = a.Str().Lower(), s2 = b.Str().Lower();
        int c = s1.view().compare(s2.view());
        return c < 0 ? -1 : c > 0 ? 1 : 0;
    }
    if ((n1 == kInfinity && n2 == kInfinity) || (n1 == -kInfinity && n2 == -kInfinity)) return 0;
    return n1 - n2;
}

double Tan(double deg)
{
    double a = std::fmod(deg, 360);
    if (a == -270 || a == 90) return kInfinity;
    if (a == -90 || a == 270) return -kInfinity;
    return Round(std::tan(kPi * a / 180) * 1e10) / 1e10;
}

static std::mt19937_64& Rng()
{
    static std::mt19937_64 rng{std::random_device{}()};
    return rng;
}

double RandomUnit() { return std::uniform_real_distribution<double>(0.0, 1.0)(Rng()); }

double PickRandom(const Value& from, const Value& to)
{
    double a = from.Num(), b = to.Num();
    double low = std::min(a, b), high = std::max(a, b);
    if (low == high) return low;
    if (from.IsInt() && to.IsInt()) return low + std::floor(RandomUnit() * (high + 1 - low));
    return RandomUnit() * (high - low) + low;
}

String LetterOf(double letter, const String& s)
{
    double i = letter - 1;
    if (!(i >= 0) || i >= s.size()) return String();
    return String::Char(s[(int)i]);
}

bool Contains(const String& a, const String& b)
{
    return a.Lower().view().find(b.Lower().view()) != std::u16string_view::npos;
}

} // namespace scratch::ops
