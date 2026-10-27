// Costumes and sounds, as listed in assets/manifest.json (written by tools/rasterize.js).
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Platform.h"
#include "ScratchList.h"
#include "Value.h"

namespace scratch {

/// A costume image plus its size and rotation center, in Scratch stage units.
struct Costume
{
    String Name;
    std::string File;
    /// Texture pixels per stage unit (SVGs were pre-rendered at high resolution).
    double Scale = 1;
    double Width = 0, Height = 0, CenterX = 0, CenterY = 0;
    unsigned Texture = 0; // OpenGL texture
    int PixelWidth = 0, PixelHeight = 0;
    std::vector<uint8_t> Alpha; // for "touching mouse-pointer?"

    void Load(const std::string& assetDir);
    /// Is the pixel at costume-space (u, v) (stage units from the top-left) not transparent?
    bool IsOpaqueAt(double u, double v) const;
};

struct SoundAsset
{
    String Name;
    std::string File;   // full path
    void* Sound = nullptr; // the decoded sound (owned by the audio engine)
    bool Loaded = false;
};

/// The costumes of one sprite (shared by its clones), with a name lookup.
struct CostumeSet
{
    std::vector<Costume> Items;
    std::unordered_map<std::u16string, int> ByName;
    int Find(const String& name) const
    {
        auto it = ByName.find(std::u16string(name.view()));
        return it == ByName.end() ? -1 : it->second;
    }
};

struct SoundSet
{
    std::vector<SoundAsset> Items;
};

/// How far loading has got (costumes, then sounds), for the loading screen.
struct LoadProgress
{
    static inline int Done = 0, Total = 0;
    static inline const char* Phase = "starting";
    /// Called after every loaded item (the engine redraws the loading screen from here).
    static inline std::function<void()> OnStep;
    static void Step()
    {
        Done++;
        if (OnStep) OnStep();
    }
    static float Fraction() { return Total > 0 ? (float)Done / Total : 0.0f; }
};

class AssetManifest
{
public:
    static AssetManifest Load(const std::string& assetDir);
    /// Costumes (loaded into textures) and sounds of a target, by its index in the project.
    std::pair<std::shared_ptr<CostumeSet>, std::shared_ptr<SoundSet>> For(int targetIndex, const std::string& assetDir) const;

private:
    struct CostumeEntry { std::string name, file; double scale, width, height, centerX, centerY; };
    struct SoundEntry { std::string name, file; };
    struct TargetEntry { std::string name; std::vector<CostumeEntry> costumes; std::vector<SoundEntry> sounds; };
    std::vector<TargetEntry> targets_;
};

/// Starting contents of lists, read from assets/initial-lists.json.
class InitialLists
{
public:
    explicit InitialLists(const std::string& assetDir);
    ~InitialLists();
    void Fill(ScratchList& list, const char* key) const;

private:
    struct Data;
    std::unique_ptr<Data> data_;
};

} // namespace scratch
