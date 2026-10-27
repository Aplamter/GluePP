#include "Value.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace scratch {

// ------------------------------------------------------------------------------------------------
// String
// ------------------------------------------------------------------------------------------------
String::Rep* String::alloc(size_t len)
{
    auto* r = static_cast<Rep*>(std::malloc(sizeof(Rep) + len * sizeof(char16_t)));
    r->refs = 1;
    r->len = (int)len;
    r->data[len] = 0;
    return r;
}

String::String(const char16_t* s, size_t len)
{
    if (len == 0) return;
    rep_ = alloc(len);
    std::memcpy(rep_->data, s, len * sizeof(char16_t));
}

void String::release() noexcept
{
    if (rep_ && --rep_->refs == 0) std::free(rep_);
    rep_ = nullptr;
}

String operator+(const String& a, const String& b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;
    String r;
    r.rep_ = String::alloc(a.size() + b.size());
    std::memcpy(r.rep_->data, a.data(), a.size() * sizeof(char16_t));
    std::memcpy(r.rep_->data + a.size(), b.data(), b.size() * sizeof(char16_t));
    return r;
}

String String::Char(char16_t c)
{
    static String* table = [] {
        auto* t = new String[256];
        for (int i = 0; i < 256; i++) { char16_t ch = (char16_t)i; t[i] = String(&ch, 1); }
        return t;
    }();
    if (c < 256) return table[c];
    return String(&c, 1);
}

static char16_t LowerChar(char16_t c)
{
    if (c < 128) return (c >= 'A' && c <= 'Z') ? c + 32 : c;
    if ((c >= 0xC0 && c <= 0xDE && c != 0xD7)) return c + 32;                  // Latin-1
    if (c >= 0x100 && c <= 0x17F && c != 0x130 && c != 0x138 && c != 0x149 && c != 0x178)
    {
        bool evenUpper = !(c >= 0x139 && c <= 0x148) && !(c >= 0x179 && c <= 0x17E);
        if (evenUpper) return (c % 2 == 0) ? c + 1 : c;
        return (c % 2 == 1) ? c + 1 : c;
    }
    if (c == 0x178) return 0xFF;
    if (c >= 0x391 && c <= 0x3AB && c != 0x3A2) return c + 32;                  // Greek
    if (c >= 0x410 && c <= 0x42F) return c + 32;                                 // Cyrillic
    if (c >= 0x400 && c <= 0x40F) return c + 80;
    return c;
}

static char16_t UpperChar(char16_t c)
{
    if (c < 128) return (c >= 'a' && c <= 'z') ? c - 32 : c;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 32;
    if (c >= 0x3B1 && c <= 0x3CB && c != 0x3C2) return c - 32;
    if (c >= 0x430 && c <= 0x44F) return c - 32;
    if (c >= 0x450 && c <= 0x45F) return c - 80;
    return c;
}

String String::Lower() const
{
    int n = size();
    int i = 0;
    while (i < n && LowerChar(data()[i]) == data()[i]) i++;
    if (i == n) return *this; // already lower case: share
    String r;
    r.rep_ = alloc(n);
    for (int k = 0; k < n; k++) r.rep_->data[k] = LowerChar(data()[k]);
    return r;
}

String String::Upper() const
{
    int n = size();
    String r;
    if (n == 0) return r;
    r.rep_ = alloc(n);
    for (int k = 0; k < n; k++) r.rep_->data[k] = UpperChar(data()[k]);
    return r;
}

std::u16string Utf8ToUtf16(std::string_view s)
{
    std::u16string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();)
    {
        unsigned char c = s[i];
        uint32_t cp;
        int extra;
        if (c < 0x80) { cp = c; extra = 0; }
        else if ((c >> 5) == 6) { cp = c & 0x1F; extra = 1; }
        else if ((c >> 4) == 14) { cp = c & 0x0F; extra = 2; }
        else if ((c >> 3) == 30) { cp = c & 0x07; extra = 3; }
        else { out.push_back(0xFFFD); i++; continue; }
        bool ok = true;
        for (int k = 1; k <= extra; k++)
        {
            if (i + k >= s.size() || ((unsigned char)s[i + k] >> 6) != 2) { ok = false; break; }
            cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
        }
        if (!ok) { out.push_back(0xFFFD); i++; continue; }
        i += extra + 1;
        if (cp >= 0x10000)
        {
            cp -= 0x10000;
            out.push_back((char16_t)(0xD800 + (cp >> 10)));
            out.push_back((char16_t)(0xDC00 + (cp & 0x3FF)));
        }
        else out.push_back((char16_t)cp);
    }
    return out;
}

std::string Utf16ToUtf8(std::u16string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++)
    {
        uint32_t cp = s[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF)
        {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (s[i + 1] - 0xDC00);
            i++;
        }
        else if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD; // lone surrogate
        if (cp < 0x80) out.push_back((char)cp);
        else if (cp < 0x800) { out.push_back((char)(0xC0 | (cp >> 6))); out.push_back((char)(0x80 | (cp & 0x3F))); }
        else if (cp < 0x10000)
        {
            out.push_back((char)(0xE0 | (cp >> 12)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        }
        else
        {
            out.push_back((char)(0xF0 | (cp >> 18)));
            out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

String String::FromUtf8(std::string_view s)
{
    bool ascii = true;
    for (char c : s) if ((unsigned char)c >= 0x80) { ascii = false; break; }
    if (ascii)
    {
        String r;
        if (s.empty()) return r;
        r.rep_ = alloc(s.size());
        for (size_t i = 0; i < s.size(); i++) r.rep_->data[i] = (unsigned char)s[i];
        return r;
    }
    auto u = Utf8ToUtf16(s);
    return String(u.data(), u.size());
}

std::string String::ToUtf8() const { return Utf16ToUtf8(view()); }

// ------------------------------------------------------------------------------------------------
// JavaScript rules
// ------------------------------------------------------------------------------------------------
namespace Js {

bool IsSpace(char16_t c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f' || c == 0x00A0 || c == 0xFEFF ||
           c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F ||
           c == 0x3000;
}

bool IsWhiteSpaceOnly(const String& s)
{
    for (int i = 0; i < s.size(); i++) if (!IsSpace(s[i])) return false;
    return true;
}

bool StringToBool(const String& s)
{
    int n = s.size();
    if (n == 0) return false;
    if (n == 1 && s[0] == '0') return false;
    if (n == 5)
    {
        static const char16_t f[] = u"false";
        for (int i = 0; i < 5; i++)
        {
            char16_t c = s[i];
            if (c >= 'A' && c <= 'Z') c += 32;
            if (c != f[i]) return true;
        }
        return false;
    }
    return true;
}

static int HexDigit(char16_t c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

static bool IsDigit(char16_t c) { return c >= '0' && c <= '9'; }

double ToNumber(std::u16string_view s)
{
    constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
    // Fast path: short plain integers.
    size_t len = s.size();
    if (len > 0 && len < 16)
    {
        size_t i = 0;
        bool neg = false;
        if (s[0] == '-') { neg = true; i = 1; }
        if (i < len)
        {
            long long v = 0;
            bool ok = true;
            for (size_t k = i; k < len; k++)
            {
                unsigned d = (unsigned)(s[k] - '0');
                if (d > 9) { ok = false; break; }
                v = v * 10 + d;
            }
            if (ok) return neg ? (v == 0 ? -0.0 : -(double)v) : (double)v;
        }
    }
    size_t a = 0, b = len;
    while (a < b && IsSpace(s[a])) a++;
    while (b > a && IsSpace(s[b - 1])) b--;
    std::u16string_view t = s.substr(a, b - a);
    if (t.empty()) return 0;
    if (t == u"Infinity" || t == u"+Infinity") return std::numeric_limits<double>::infinity();
    if (t == u"-Infinity") return -std::numeric_limits<double>::infinity();
    if (t.size() > 2 && t[0] == '0')
    {
        char16_t p = t[1];
        int radix = (p == 'x' || p == 'X') ? 16 : (p == 'o' || p == 'O') ? 8 : (p == 'b' || p == 'B') ? 2 : 0;
        if (radix)
        {
            double v = 0;
            for (size_t k = 2; k < t.size(); k++)
            {
                int d = HexDigit(t[k]);
                if (d < 0 || d >= radix) return NaN;
                v = v * radix + d;
            }
            return v;
        }
    }
    // StrDecimalLiteral: [+-] (digits [. digits] | . digits) [e [+-] digits]
    size_t j = 0, n = t.size();
    if (t[j] == '+' || t[j] == '-') j++;
    int intDigits = 0, fracDigits = 0;
    while (j < n && IsDigit(t[j])) { j++; intDigits++; }
    if (j < n && t[j] == '.') { j++; while (j < n && IsDigit(t[j])) { j++; fracDigits++; } }
    if (intDigits + fracDigits == 0) return NaN;
    if (j < n && (t[j] == 'e' || t[j] == 'E'))
    {
        j++;
        if (j < n && (t[j] == '+' || t[j] == '-')) j++;
        int expDigits = 0;
        while (j < n && IsDigit(t[j])) { j++; expDigits++; }
        if (expDigits == 0) return NaN;
    }
    if (j != n) return NaN;
    // All ASCII now; from_chars doesn't accept a leading '+' or a bare trailing '.'.
    std::string ascii;
    ascii.reserve(n + 1);
    size_t k = 0;
    bool neg = false;
    if (t[0] == '+' || t[0] == '-') { neg = t[0] == '-'; k = 1; }
    if (t[k] == '.') ascii.push_back('0');
    for (; k < n; k++) ascii.push_back((char)t[k]);
    double v = 0;
    auto res = std::from_chars(ascii.data(), ascii.data() + ascii.size(), v);
    if (res.ec == std::errc::result_out_of_range)
    {
        // Overflow -> Infinity, underflow -> 0 (what JavaScript does).
        bool big = false;
        size_t e = ascii.find_first_of("eE");
        if (e == std::string::npos) big = true;
        else big = ascii[e + 1] != '-';
        v = big ? std::numeric_limits<double>::infinity() : 0.0;
    }
    return neg ? -v : v;
}

double ToNumber(const String& s) { return ToNumber(s.view()); }

std::string NumberToUtf8(double d)
{
    if (d != d) return "NaN";
    if (d == std::numeric_limits<double>::infinity()) return "Infinity";
    if (d == -std::numeric_limits<double>::infinity()) return "-Infinity";
    if (d == 0) return "0";
    char buf[64];
    if (d == std::trunc(d) && std::fabs(d) < 1e15)
    {
        auto r = std::to_chars(buf, buf + sizeof buf, (long long)d);
        return std::string(buf, r.ptr);
    }
    // Shortest round-trip digits in scientific form: d.ddddde[+-]x
    auto r = std::to_chars(buf, buf + sizeof buf, d, std::chars_format::scientific);
    std::string sci(buf, r.ptr);
    bool neg = sci[0] == '-';
    if (neg) sci.erase(0, 1);
    size_t e = sci.find('e');
    std::string mant = sci.substr(0, e);
    int exp = std::atoi(sci.c_str() + e + 1);
    std::string digits;
    for (char c : mant) if (c != '.') digits.push_back(c);
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    int k = (int)digits.size();
    int n = exp + 1; // decimal point position relative to the digits
    std::string body;
    if (k <= n && n <= 21) body = digits + std::string(n - k, '0');
    else if (0 < n && n <= 21) body = digits.substr(0, n) + "." + digits.substr(n);
    else if (-6 < n && n <= 0) body = "0." + std::string(-n, '0') + digits;
    else
    {
        int ee = n - 1;
        std::string es = (ee >= 0 ? "+" : "-") + std::to_string(ee < 0 ? -ee : ee);
        body = k == 1 ? digits + "e" + es : digits.substr(0, 1) + "." + digits.substr(1) + "e" + es;
    }
    return neg ? "-" + body : body;
}

String NumberToString(double d)
{
    // Small whole numbers are very common (list indexes, coordinates): cache them.
    if (d >= 0 && d < 1024 && d == (int)d && !(d == 0 && std::signbit(d)))
    {
        static String* cache = [] {
            auto* c = new String[1024];
            for (int i = 0; i < 1024; i++) c[i] = String::FromUtf8(std::to_string(i));
            return c;
        }();
        return cache[(int)d];
    }
    return String::FromUtf8(NumberToUtf8(d));
}

} // namespace Js

// ------------------------------------------------------------------------------------------------
// Value
// ------------------------------------------------------------------------------------------------
const Value Value::Empty{};

double Value::Num() const
{
    switch (type_)
    {
    case ValueType::Number: return num_ != num_ ? 0 : num_;
    case ValueType::Boolean: return num_;
    default: return Js::ToNumberOrZero(str_);
    }
}

double Value::JsNumber() const
{
    if (type_ == ValueType::String) return Js::ToNumber(str_);
    return num_;
}

String Value::Str() const
{
    switch (type_)
    {
    case ValueType::String: return str_;
    case ValueType::Number: return Js::NumberToString(num_);
    default: return num_ != 0 ? u"true"_s : u"false"_s;
    }
}

bool Value::Bool() const
{
    switch (type_)
    {
    case ValueType::Boolean: return num_ != 0;
    case ValueType::Number: return num_ != 0 && num_ == num_;
    default: return Js::StringToBool(str_);
    }
}

bool Value::IsWhiteSpace() const { return type_ == ValueType::String && Js::IsWhiteSpaceOnly(str_); }

bool Value::IsInt() const
{
    switch (type_)
    {
    case ValueType::Number: return num_ != num_ || num_ == std::trunc(num_);
    case ValueType::Boolean: return true;
    default: return str_.view().find(u'.') == std::u16string_view::npos;
    }
}

Value Value::FromLiteral(const String& s)
{
    double n = Js::ToNumber(s);
    if (n == n && Js::NumberToString(n) == s) return Value(n);
    return Value(s);
}

} // namespace scratch
