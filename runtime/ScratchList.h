// A Scratch list, with Scratch's 1-based index rules.
#pragma once
#include <cmath>
#include <vector>

#include "Value.h"

namespace scratch {

class ScratchList
{
public:
    String Name;
    std::vector<Value> Items;
    /// Bumped on every change so list monitors know when to redraw.
    int Version = 0;

    explicit ScratchList(String name = {}) : Name(std::move(name)) {}

    Value Item(const Value& index) const;
    /// Fast path for numeric indexes.
    Value Item(double index) const
    {
        if (index != index) index = 0;
        double i = std::floor(index);
        return (i >= 1 && i <= (double)Items.size()) ? Items[(size_t)i - 1] : Value();
    }
    Value Item(int index) const { return Item((double)index); }

    double Length() const { return (double)Items.size(); }

    void Add(const Value& v) { Items.push_back(v); Version++; }
    void Delete(const Value& index);
    void DeleteAll() { Items.clear(); Version++; }
    void Insert(const Value& index, const Value& item);
    void Replace(const Value& index, const Value& item);
    /// "item # of": 1-based position or 0.
    double IndexOf(const Value& item) const;
    bool Contains(const Value& item) const;
    /// The list reporter: items joined by spaces, or with nothing if all are single characters.
    String Contents() const;

private:
    static constexpr int Invalid = -1, All = -2;
    int ToIndex(const Value& index, bool acceptAll) const;
};

} // namespace scratch
