// Hand-written: the build settings a game player reads from data/build.json (shared by editor and player).
#pragma once
#include <string>

namespace Glue3D {

struct PlayerSettings
{
    std::string Title = "Glue3D game";
    bool Fullscreen = false;
    bool Intro = true;
    /// Picture shown while the game loads ("" = a black screen). In a build: a file in data/.
    std::string LoadingImage;
    /// The picture's original file name (shown in the build dialog).
    std::string LoadingImageName;

    /// Reads a settings file; missing or broken files give the defaults.
    static PlayerSettings Load(const std::string& path);
    bool Save(const std::string& path) const;
};

} // namespace Glue3D
