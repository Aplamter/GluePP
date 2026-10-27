#include "ScratchList.h"

#include <cmath>

#include "Ops.h"

namespace scratch {

int ScratchList::ToIndex(const Value& index, bool acceptAll) const
{
    int length = (int)Items.size();
    if (!index.IsNumber())
    {
        String s = index.Str();
        if (s == u"all") return acceptAll ? All : Invalid;
        if (s == u"last") return length > 0 ? length : Invalid;
        if (s == u"random" || s == u"any") return length > 0 ? 1 + (int)std::floor(ops::RandomUnit() * length) : Invalid;
    }
    double i = std::floor(index.Num());
    if (i < 1 || i > length) return Invalid;
    return (int)i;
}

Value ScratchList::Item(const Value& index) const
{
    if (index.IsNumber()) return Item(index.RawNumber());
    int i = ToIndex(index, false);
    return i == Invalid ? Value() : Items[i - 1];
}

void ScratchList::Delete(const Value& index)
{
    int i = ToIndex(index, true);
    if (i == Invalid) return;
    if (i == All) Items.clear();
    else Items.erase(Items.begin() + (i - 1));
    Version++;
}

void ScratchList::Insert(const Value& index, const Value& item)
{
    int length = (int)Items.size();
    int i;
    String s = index.IsNumber() ? String() : index.Str();
    if (!index.IsNumber() && s == u"last") i = length + 1;
    else if (!index.IsNumber() && (s == u"random" || s == u"any")) i = 1 + (int)std::floor(ops::RandomUnit() * (length + 1));
    else
    {
        double d = std::floor(index.Num());
        if (d < 1 || d > length + 1) return;
        i = (int)d;
    }
    Items.insert(Items.begin() + (i - 1), item);
    Version++;
}

void ScratchList::Replace(const Value& index, const Value& item)
{
    int i = ToIndex(index, false);
    if (i == Invalid) return;
    Items[i - 1] = item;
    Version++;
}

double ScratchList::IndexOf(const Value& item) const
{
    for (size_t i = 0; i < Items.size(); i++)
        if (ops::Compare(Items[i], item) == 0) return (double)(i + 1);
    return 0;
}

bool ScratchList::Contains(const Value& item) const
{
    for (const auto& v : Items)
        if (ops::Compare(v, item) == 0) return true;
    return false;
}

String ScratchList::Contents() const
{
    std::vector<String> parts;
    parts.reserve(Items.size());
    bool allSingle = true;
    size_t total = 0;
    for (const auto& v : Items)
    {
        parts.push_back(v.Str());
        if (parts.back().size() != 1) allSingle = false;
        total += parts.back().size() + 1;
    }
    std::u16string out;
    out.reserve(total);
    for (size_t i = 0; i < parts.size(); i++)
    {
        if (i > 0 && !allSingle) out.push_back(u' ');
        out.append(parts[i].view());
    }
    return String(out);
}

} // namespace scratch
