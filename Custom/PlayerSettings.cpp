#include "PlayerSettings.h"

#include <nlohmann/json.hpp>

#include "Platform.h"

namespace Glue3D {

PlayerSettings PlayerSettings::Load(const std::string& path)
{
    PlayerSettings s;
    std::string text;
    if (!scratch::ReadFile(path, text)) return s;
    try
    {
        auto j = nlohmann::json::parse(text);
        s.Title = j.value("Title", s.Title);
        s.Fullscreen = j.value("Fullscreen", s.Fullscreen);
        s.Intro = j.value("Intro", s.Intro);
        s.LoadingImage = j.value("LoadingImage", s.LoadingImage);
        s.LoadingImageName = j.value("LoadingImageName", s.LoadingImageName);
    }
    catch (...)
    {
        // use the defaults
    }
    return s;
}

bool PlayerSettings::Save(const std::string& path) const
{
    nlohmann::json j = {{"Title", Title}, {"Fullscreen", Fullscreen}, {"Intro", Intro}, {"LoadingImage", LoadingImage},
                        {"LoadingImageName", LoadingImageName}};
    return scratch::WriteFileAtomic(path, j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace));
}

} // namespace Glue3D
