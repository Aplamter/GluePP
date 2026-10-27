// Hand-written (not overwritten by the converter): File > build.. turns the open project into a game .exe.
//
// How it works (the same way Unity and Godot build games):
//   - Glue3D comes with a prebuilt *game player* in its "player" folder (CMake target Glue3DPlayer).
//     It is compiled from the same code, but with BuildFlavor::IsEditor = false, so the editor isn't in it at all.
//   - "build" copies that player as <project>.exe and puts the game next to it:
//
//       Projects/my-game/build/
//         my-game.exe          <- the game player
//         data/project.glue3d  <- your game (the project, saved just before building)
//         data/build.json      <- build settings (title, fullscreen, intro)
//         data/assets/         <- costumes, sounds, font
//
//   - The player opens the project in Glue3D's own "exported game" mode: fullscreen game view, the game
//     starts by itself, and no editor.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <regex>
#include <set>
#include <stdexcept>

#include "Engine.h"
#include "Glue3DSprite.g.h"
#include "PlayerSettings.h"
#include "ProjectFiles.h"
#include "stb_image.h"

namespace Glue3D {

namespace fs = std::filesystem;

namespace {

Value T(const std::string& s) { return Value(String::FromUtf8(s)); }
const Value E;

#ifdef _WIN32
constexpr const char* PlayerFileName = "Glue3DPlayer.exe";
constexpr const char* ExeSuffix = ".exe";
#else
constexpr const char* PlayerFileName = "Glue3DPlayer";
constexpr const char* ExeSuffix = "";
#endif

std::string PlayerFolder() { return scratch::PathJoin(scratch::ExeDir(), "player"); }

struct BuildError : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

} // namespace

/// File > build..
void Glue3DSprite::OpenBuildDialog()
{
    if (ProjectFolder.empty() || ProjectName.empty())
    {
        Notify(u"no project is open"_s, u"_icon.warn"_s, u"Error_1"_s, 1);
        return;
    }
    LoadBuildSettings();
    buildDialogOpen_ = true;
    buildStep_ = 0;
    stage.guiFreeze = 1; // freeze the editor underneath, like Glue3D's own dialogs
}

void Glue3DSprite::CloseBuildDialog()
{
    buildDialogOpen_ = false;
    buildStep_ = 0;
    stage.guiFreeze = 0;
}

void Glue3DSprite::BuildDialogFrame()
{
    if (!buildDialogOpen_) return;
    if (buildStep_ == 2) { RunBuild(); return; }

    // Glue3D's dialog trick: unfreeze while drawing our own buttons so they react, freeze again after.
    stage.guiFreeze = 1 - stage.guiFreeze.Num();
    bool live = buildStep_ == 0;
    double i = live ? 1 : 0;

    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_gui.dark"_s, 0, 0, 90, 100, 0, 0, 0, 0, E, E, 0, E);
    RenderBoxToBoxTypeBorderAlternate(-100, 90, 100, -165, 1, 2, Num(Gt(stage.guiFocusEffect, 0)) * 1, E, E);
    Label("build", -95, 83, 0.55, false, 75);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_button.delete"_s, 90, 80, 90, 38, 30, 0, 0, i, E, E, 1, E);
    if (Eq(stage.guiClick, 1))
    {
        stage.guiFreeze = 1 - stage.guiFreeze.Num();
        CloseBuildDialog();
        return;
    }

    Label("build your game", 0, 62, 0.67, true);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_icon.game"_s, -65, 35, 90, 70, 10, 0, 0, i, E, E, 1, u"game"_s);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_gui.info"_s, 14, 35, 90, 35, 50, -100, 0, E, E, E, 0, E);
    std::string exe = ProjectName + ExeSuffix;
    double len = String::FromUtf8(exe).Length();
    TextOnScaleCenterGbcCursorLimitScroll(T(exe), -28, 34.5, 0.7 + Num(Gt(len, 8)) * ((len - 9) * -0.02), E, E, 25, 30, -kInfinity, E, E, E, E);

    Label("build settings", 0, 8, 0.62, true);
    BuildToggle("start in fullscreen", "_icon.ui", -12, buildFullscreen_, "f11 switches it in the game", i);
    BuildToggle("play the glue3d intro", "_icon.star", -42, buildIntro_, "the logo animation at start", i);

    // Custom loading screen: a picture shown while the game loads (a black screen without one).
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_icon.texture"_s, -70, -72, 90, 45, 10, 0, 0, E, E, E, 1, E);
    TextOnScaleCenterGbcCursorLimitScroll(u"custom loading screen"_s, -52, -68, 0.5, E, E, 25, 30, -kInfinity, E, E, E, E);
    std::string shown = buildLoadingImage_.empty() ? "none: a black screen" : buildLoadingImageName_;
    for (char& ch : shown) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
    if (shown.size() > 24) shown = shown.substr(0, 23) + "~";
    InfoText(shown, -52, -79, 0.38);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_icon.filemove"_s, buildLoadingImage_.empty() ? 70 : 62, -72, 90, 38, 30, 0, 0, i, E, E, 1,
                                                      u"choose a png or jpg"_s);
    if (Eq(stage.guiClick, 1)) PickLoadingImage();
    if (!buildLoadingImage_.empty())
    {
        Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_button.delete"_s, 86, -72, 90, 28, 30, 0, 0, i, E, E, 1, u"remove"_s);
        if (Eq(stage.guiClick, 1)) ClearLoadingImage();
    }

    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_button.info"_s, -90, -145, 90, 28, 40, 0, 0, i, E, E, 1, E);
    InfoText("only the game is built, without the editor", -79, -141, 0.42);
    InfoText("into your project's build folder", -79, -152, 0.42);

    if (live)
    {
        Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_button.proceed"_s, 75, -112, 90, 52, 5, -10, 0, 1, E, E, 1, u"build"_s);
        if (Eq(stage.guiClick, 1))
        {
            SaveBuildSettings();
            buildStep_ = 1; // draw "building..." first, then build on the next frame
        }
    }
    else
    {
        InfoText("building...", 40, -112, 0.6);
        buildStep_ = 2;
    }
    stage.guiFreeze = 1 - stage.guiFreeze.Num();
}

void Glue3DSprite::BuildToggle(const char* label, const char* icon, double y, bool& on, const char* hint, double interactive)
{
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(T(icon), -70, y, 90, 45, 10, 0, 0, E, E, E, 1, E);
    TextOnScaleCenterGbcCursorLimitScroll(T(label), -52, y + 4, 0.5, E, E, 25, 30, -kInfinity, E, E, E, E);
    InfoText(hint, -52, y - 7, 0.38);
    // Glue3D's toggle style (as in its settings): the check is greyed out when off.
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_button.accept"_s, 70, y, 90, 35, 40, 0, on ? 0 : -kInfinity, interactive, E, 1, 1,
                                                      on ? u"on"_s : u"off"_s);
    if (Eq(stage.guiClick, 1)) on = !on;
}

// ---------------- the actual build ----------------

static void CopyFolder(const fs::path& from, const fs::path& to)
{
    fs::create_directories(to);
    for (const auto& entry : fs::directory_iterator(from))
    {
        fs::path dest = to / entry.path().filename();
        if (entry.is_directory()) CopyFolder(entry.path(), dest);
        else fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing);
    }
}

/// Levels: every project named in a game.load_scene(str name) of this project (and of those levels, and so on)
/// goes into data/levels/<name>/, where the built game loads it from.
void Glue3DSprite::CopyLevels(const fs::path& data)
{
    static const std::regex use(R"(load_scene\s*\(\s*str\s+([^,)\s\\"]+))");
    std::set<std::string> done;
    std::vector<std::string> todo{scratch::PathJoin(ProjectFolder, ProjectFiles::FileName)};
    while (!todo.empty())
    {
        std::string file = todo.back();
        todo.pop_back();
        std::string text;
        if (!scratch::ReadFile(file, text)) continue;
        for (std::sregex_iterator it(text.begin(), text.end(), use), end; it != end; ++it)
        {
            std::string name = (*it)[1];
            if (!done.insert(name).second) continue;
            std::string folder = scratch::PathJoin(ProjectFiles::ProjectsRoot(), name);
            if (!ProjectFiles::IsProjectFolder(folder))
            {
                std::fprintf(stderr, "build: level \"%s\" not found in the projects folder\n", name.c_str());
                continue;
            }
            fs::path dest = data / "levels" / scratch::FsPath(name);
            fs::create_directories(dest);
            fs::path src = scratch::FsPath(scratch::PathJoin(folder, ProjectFiles::FileName));
            fs::copy_file(src, dest / ProjectFiles::FileName, fs::copy_options::overwrite_existing);
            todo.push_back(scratch::PathJoin(folder, ProjectFiles::FileName));
        }
    }
}

void Glue3DSprite::RunBuild()
{
    std::string error;
    std::string outFolder = scratch::PathJoin(ProjectFolder, "build");
    try
    {
        fs::path playerDir = scratch::FsPath(PlayerFolder());
        fs::path player = playerDir / PlayerFileName;
        if (!fs::exists(player)) throw BuildError("the game player is missing (player folder next to glue3d)");

        // 1. Save, so the game is exactly what's in the editor now.
        ProjectFiles::Save(ProjectFolder, ProjectName, stage.ramEngineData.Item(72).Str().ToUtf8(), stage);

        // 2. Fresh build folder.
        fs::path out = scratch::FsPath(outFolder);
        fs::remove_all(out);
        fs::path data = out / "data";
        fs::create_directories(data);

        // 3. The player becomes <project>.exe (plus anything that ships beside it).
        for (const auto& entry : fs::directory_iterator(playerDir))
        {
            if (!entry.is_regular_file()) continue;
            bool isPlayer = entry.path().filename() == PlayerFileName;
            fs::path dest = out / (isPlayer ? scratch::FsPath(ProjectName + ExeSuffix) : entry.path().filename());
            fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing);
            fs::permissions(dest, fs::status(entry.path()).permissions()); // keeps it runnable on Linux/macOS
        }

        // 4. The game: project data, build settings, and the art/sounds/font it draws with.
        fs::copy_file(scratch::FsPath(scratch::PathJoin(ProjectFolder, ProjectFiles::FileName)), data / ProjectFiles::FileName,
                      fs::copy_options::overwrite_existing);
        PlayerSettings settings;
        settings.Title = ProjectName;
        settings.Fullscreen = buildFullscreen_;
        settings.Intro = buildIntro_;
        // The custom loading screen picture goes into data/ too.
        fs::path picture = scratch::FsPath(scratch::PathJoin(ProjectFolder, buildLoadingImage_));
        if (!buildLoadingImage_.empty() && fs::exists(picture))
        {
            std::string name = "loading" + scratch::PathText(picture.extension());
            fs::copy_file(picture, data / name, fs::copy_options::overwrite_existing);
            settings.LoadingImage = name;
        }
        if (!settings.Save(scratch::PathText(data / "build.json"))) throw BuildError("could not write build.json");
        CopyLevels(data);
        CopyFolder(scratch::FsPath(GetEngine().AssetDir), data / "assets");
    }
    catch (const BuildError& e)
    {
        error = e.what();
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "build failed: %s\n", e.what());
        error = "the build failed: could not write the files";
    }

    CloseBuildDialog();
    if (!error.empty())
    {
        Notify(T(error), u"_icon.warn"_s, u"Error_3"_s, E);
        return;
    }
    Notify(T("build complete: " + ProjectName + "/build"), u"_icon.game"_s, u"Windows XP Print complete"_s, 1);
    if (!std::getenv("SCRATCH_AUTOMATION")) scratch::OpenInFileBrowser(outFolder); // not during automated tests
}

// ---------------- custom loading screen ----------------

/// The system's "open file" window; the picture is copied into the project folder (so the project keeps it).
void Glue3DSprite::PickLoadingImage()
{
    std::string file = scratch::PickImageFile("Choose a loading screen picture (png or jpg)");
    if (file.empty()) return; // cancelled
    fs::path src = scratch::FsPath(file);
    std::string ext = scratch::PathText(src.extension());
    for (char& ch : ext) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
    if (ext != ".png" && ext != ".jpg" && ext != ".jpeg")
    {
        Notify(u"please choose a png or jpg picture"_s, u"_icon.warn"_s, u"Error_1"_s, 1);
        return;
    }
    int w, h, n;
    std::string bytes;
    if (!scratch::ReadFile(file, bytes) || !stbi_info_from_memory((const unsigned char*)bytes.data(), (int)bytes.size(), &w, &h, &n))
    {
        Notify(u"that picture can't be opened"_s, u"_icon.warn"_s, u"Error_1"_s, 1);
        return;
    }
    try
    {
        ClearLoadingImage();
        std::string name = "loading-screen" + ext;
        fs::copy_file(src, scratch::FsPath(scratch::PathJoin(ProjectFolder, name)), fs::copy_options::overwrite_existing);
        buildLoadingImage_ = name;
        buildLoadingImageName_ = scratch::PathText(src.filename());
        SaveBuildSettings();
        StartSound(u"Windows Vista Beta Balloon"_s);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "%s\n", e.what());
        Notify(u"the picture could not be copied"_s, u"_icon.warn"_s, u"Error_3"_s, E);
    }
}

void Glue3DSprite::ClearLoadingImage()
{
    std::error_code ec;
    if (!buildLoadingImage_.empty()) fs::remove(scratch::FsPath(scratch::PathJoin(ProjectFolder, buildLoadingImage_)), ec);
    buildLoadingImage_.clear();
    buildLoadingImageName_.clear();
    SaveBuildSettings();
}

// ---------------- build settings are remembered per project ----------------

void Glue3DSprite::LoadBuildSettings()
{
    std::string file = scratch::PathJoin(ProjectFolder, "build-settings.json");
    std::error_code ec;
    if (!fs::exists(scratch::FsPath(file), ec)) return;
    PlayerSettings s = PlayerSettings::Load(file);
    buildFullscreen_ = s.Fullscreen;
    buildIntro_ = s.Intro;
    buildLoadingImage_ = s.LoadingImage;
    buildLoadingImageName_ = s.LoadingImageName;
}

void Glue3DSprite::SaveBuildSettings()
{
    PlayerSettings s;
    s.Title = ProjectName;
    s.Fullscreen = buildFullscreen_;
    s.Intro = buildIntro_;
    s.LoadingImage = buildLoadingImage_;
    s.LoadingImageName = buildLoadingImageName_;
    s.Save(scratch::PathJoin(ProjectFolder, "build-settings.json")); // not important if it fails
}

} // namespace Glue3D
