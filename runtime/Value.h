// Scratch values: numbers, text and true/false, converted between each other exactly like Scratch
// (Scratch runs on JavaScript, so text is UTF-16 and numbers print the way JavaScript prints them).
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

namespace scratch {

// ------------------------------------------------------------------------------------------------
// String: immutable, reference-counted UTF-16 text. Copying one is just a counter increment.
// ------------------------------------------------------------------------------------------------
class String
{
public:
    String() noexcept = default;
    String(const char16_t* s, size_t len);
    String(std::u16string_view s) : String(s.data(), s.size()) {}
    String(const String& o) noexcept : rep_(o.rep_) { if (rep_) ++rep_->refs; }
    String(String&& o) noexcept : rep_(o.rep_) { o.rep_ = nullptr; }
    ~String() { release(); }
    String& operator=(const String& o) noexcept
    {
        if (o.rep_) ++o.rep_->refs;
        release();
        rep_ = o.rep_;
        return *this;
    }
    String& operator=(String&& o) noexcept
    {
        if (this != &o) { release(); rep_ = o.rep_; o.rep_ = nullptr; }
        return *this;
    }

    /// Text from UTF-8 (file names, JSON, C++ string literals).
    static String FromUtf8(std::string_view s);
    std::string ToUtf8() const;

    /// A one-character string (cached for the first 256 code units).
    static String Char(char16_t c);

    int size() const noexcept { return rep_ ? rep_->len : 0; }
    bool empty() const noexcept { return size() == 0; }
    const char16_t* data() const noexcept { return rep_ ? rep_->data : u""; }
    char16_t operator[](int i) const noexcept { return rep_->data[i]; }
    std::u16string_view view() const noexcept { return {data(), (size_t)size()}; }

    /// Scratch's "length of": counts UTF-16 code units, like JavaScript.
    double Length() const noexcept { return size(); }

    bool operator==(const String& o) const noexcept
    {
        if (rep_ == o.rep_) return true;
        int n = size();
        return n == o.size() && std::memcmp(data(), o.data(), n * sizeof(char16_t)) == 0;
    }
    bool operator!=(const String& o) const noexcept { return !(*this == o); }
    bool operator==(std::u16string_view o) const noexcept { return view() == o; }

    String Lower() const;
    String Upper() const;
    String Substr(int start, int len) const { return String(data() + start, len); }
    bool StartsWith(std::u16string_view p) const { return view().substr(0, p.size()) == p; }

    friend String operator+(const String& a, const String& b);

private:
    struct Rep { int refs; int len; char16_t data[1]; };
    Rep* rep_ = nullptr;
    void release() noexcept;
    static Rep* alloc(size_t len);
};

/// Compile-time string literal holder used by the _s literal.
template <size_t N>
struct Literal
{
    char16_t chars[N];
    constexpr Literal(const char16_t (&s)[N]) { for (size_t i = 0; i < N; i++) chars[i] = s[i]; }
};

/// u"text"_s: a Scratch string literal. Built once, then shared (no allocation when used again).
template <Literal L>
inline const String& operator""_s()
{
    static const String s(L.chars, sizeof(L.chars) / sizeof(char16_t) - 1);
    return s;
}

// ------------------------------------------------------------------------------------------------
// Value
// ------------------------------------------------------------------------------------------------
enum class ValueType : uint8_t { Number, String, Boolean };

class Value
{
public:
    Value() noexcept : type_(ValueType::String), num_(0) {}                // Scratch's "" (empty)
    Value(double n) noexcept : type_(ValueType::Number), num_(n) {}
    Value(int n) noexcept : type_(ValueType::Number), num_(n) {}
    Value(bool b) noexcept : type_(ValueType::Boolean), num_(b ? 1 : 0) {}
    Value(const String& s) noexcept : type_(ValueType::String), num_(0), str_(s) {}
    Value(String&& s) noexcept : type_(ValueType::String), num_(0), str_(std::move(s)) {}
    Value(const char*) = delete; // use u"..."_s
    Value(const char16_t*) = delete;

    static const Value Empty;

    ValueType Type() const noexcept { return type_; }
    bool IsNumber() const noexcept { return type_ == ValueType::Number; }
    bool IsString() const noexcept { return type_ == ValueType::String; }
    bool IsBoolean() const noexcept { return type_ == ValueType::Boolean; }
    double RawNumber() const noexcept { return num_; }
    const String& RawString() const noexcept { return str_; }

    /// Scratch's "toNumber": anything that isn't a valid number becomes 0.
    double Num() const;
    /// JavaScript's Number(value): like Num, but invalid text gives NaN.
    double JsNumber() const;
    /// Scratch's "toString".
    String Str() const;
    /// Scratch's "toBoolean".
    bool Bool() const;
    /// Scratch's isWhiteSpace check, used by comparisons.
    bool IsWhiteSpace() const;
    /// Scratch's Cast.isInt (used by "pick random").
    bool IsInt() const;

    /// Text that round-trips exactly through a number ("12", "-0.5") is stored as a number.
    static Value FromLiteral(const String& s);

private:
    ValueType type_;
    double num_;
    String str_;
};

// ------------------------------------------------------------------------------------------------
// JavaScript number <-> text rules
// ------------------------------------------------------------------------------------------------
namespace Js {
bool IsSpace(char16_t c);
bool IsWhiteSpaceOnly(const String& s);
bool StringToBool(const String& s);
/// JavaScript Number(string).
double ToNumber(const String& s);
double ToNumber(std::u16string_view s);
inline double ToNumberOrZero(const String& s) { double n = ToNumber(s); return n != n ? 0 : n; }
/// JavaScript Number.prototype.toString(): shortest round-trip digits, JS exponent rules.
String NumberToString(double d);
std::string NumberToUtf8(double d);
} // namespace Js

std::u16string Utf8ToUtf16(std::string_view s);
std::string Utf16ToUtf8(std::u16string_view s);

} // namespace scratch
