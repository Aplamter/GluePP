#include "Assets.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "Gfx.h"
#include "stb_image.h"

namespace scratch {

using json = nlohmann::json;

void Costume::Load(const std::string& assetDir)
{
    std::string path = PathJoin(assetDir, File);
    std::string bytes;
    int w = 0, h = 0, n = 0;
    unsigned char* px = nullptr;
    if (ReadFile(path, bytes))
        px = stbi_load_from_memory((const unsigned char*)bytes.data(), (int)bytes.size(), &w, &h, &n, 4);
    if (!px)
    {
        std::fprintf(stderr, "[assets] could not load %s\n", path.c_str());
        uint8_t clear[4] = {0, 0, 0, 0};
        Texture = gfx::CreateTexture(1, 1, clear, false);
        PixelWidth = PixelHeight = 1;
        Alpha.assign(1, 0);
        return;
    }
    PixelWidth = w;
    PixelHeight = h;
    // Keep the alpha channel for "touching mouse-pointer?".
    Alpha.resize((size_t)w * h);
    for (size_t i = 0; i < Alpha.size(); i++) Alpha[i] = px[i * 4 + 3];
    // Scratch's renderer keeps textures premultiplied; that also makes scaled-down edges correct.
    for (size_t i = 0; i < Alpha.size(); i++)
    {
        float a = px[i * 4 + 3] / 255.0f;
        px[i * 4] = (unsigned char)(px[i * 4] * a);
        px[i * 4 + 1] = (unsigned char)(px[i * 4 + 1] * a);
        px[i * 4 + 2] = (unsigned char)(px[i * 4 + 2] * a);
    }
    Texture = gfx::CreateTexture(w, h, px, true);
    stbi_image_free(px);
}

bool Costume::IsOpaqueAt(double u, double v) const
{
    if (Alpha.empty()) return false;
    int x = (int)std::floor(u * Scale), y = (int)std::floor(v * Scale);
    if (x < 0 || y < 0 || x >= PixelWidth || y >= PixelHeight) return false;
    return Alpha[(size_t)y * PixelWidth + x] > 0;
}

AssetManifest AssetManifest::Load(const std::string& assetDir)
{
    std::string text;
    if (!ReadFile(PathJoin(assetDir, "manifest.json"), text))
        throw std::runtime_error("assets/manifest.json is missing (keep the assets folder next to the program)");
    json j = json::parse(text);
    AssetManifest m;
    for (auto& t : j["targets"])
    {
        TargetEntry te;
        te.name = t.value("name", "");
        for (auto& c : t["costumes"])
            te.costumes.push_back({c.value("name", ""), c.value("file", ""), c.value("scale", 1.0), c.value("width", 0.0),
                                   c.value("height", 0.0), c.value("centerX", 0.0), c.value("centerY", 0.0)});
        for (auto& s : t["sounds"]) te.sounds.push_back({s.value("name", ""), s.value("file", "")});
        m.targets_.push_back(std::move(te));
    }
    LoadProgress::Total = 0;
    for (auto& t : m.targets_) LoadProgress::Total += (int)(t.costumes.size() + t.sounds.size());
    return m;
}

std::pair<std::shared_ptr<CostumeSet>, std::shared_ptr<SoundSet>> AssetManifest::For(int targetIndex, const std::string& assetDir) const
{
    const TargetEntry& t = targets_.at(targetIndex);
    auto costumes = std::make_shared<CostumeSet>();
    costumes->Items.resize(t.costumes.size());
    for (size_t i = 0; i < t.costumes.size(); i++)
    {
        const auto& e = t.costumes[i];
        Costume& c = costumes->Items[i];
        c.Name = String::FromUtf8(e.name);
        c.File = e.file;
        c.Scale = e.scale;
        c.Width = e.width;
        c.Height = e.height;
        c.CenterX = e.centerX;
        c.CenterY = e.centerY;
        LoadProgress::Phase = "costumes";
        c.Load(assetDir);
        LoadProgress::Step();
        costumes->ByName.emplace(std::u16string(c.Name.view()), (int)i); // the first costume with a name wins
    }
    auto sounds = std::make_shared<SoundSet>();
    for (const auto& e : t.sounds)
    {
        SoundAsset s;
        s.Name = String::FromUtf8(e.name);
        s.File = PathJoin(assetDir, e.file);
        sounds->Items.push_back(std::move(s));
    }
    return {costumes, sounds};
}

// ---------------- initial list contents ----------------

struct InitialLists::Data
{
    json j;
};

InitialLists::InitialLists(const std::string& assetDir) : data_(std::make_unique<Data>())
{
    std::string text;
    if (ReadFile(PathJoin(assetDir, "initial-lists.json"), text)) data_->j = json::parse(text);
}

InitialLists::~InitialLists() = default;

void InitialLists::Fill(ScratchList& list, const char* key) const
{
    auto it = data_->j.find(key);
    if (it == data_->j.end()) return;
    std::vector<Value> items;
    items.reserve(it->size());
    for (auto& v : *it)
    {
        if (v.is_number()) items.emplace_back(v.get<double>());
        else if (v.is_boolean()) items.emplace_back(v.get<bool>());
        else items.push_back(Value::FromLiteral(String::FromUtf8(v.get_ref<const std::string&>())));
    }
    list.Items = std::move(items);
    list.Version++;
}

} // namespace scratch
