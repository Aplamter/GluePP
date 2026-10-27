// The game player: what File > build.. turns into <project>.exe.
// It contains the Scratch runtime and Glue3D's game code, but no editor (see Custom/BuildFlavor.h).
#include <cstdio>
#include <sstream>

#include "Automation.h"
#include "Engine.h"
#include "Glue3DSprite.g.h"
#include "LoadingScreen.h"
#include "PlayerSettings.h"
#include "Project.g.h"
#include "ProjectFiles.h"

using namespace scratch;

int main()
{
    std::string dataDir = PathJoin(ExeDir(), "data");
    Glue3D::PlayerSettings settings = Glue3D::PlayerSettings::Load(PathJoin(dataDir, "build.json"));

    Engine engine;
    engine.AssetDir = PathJoin(dataDir, "assets");
    engine.StartFullscreen = settings.Fullscreen;
    // While the game loads: a black screen, or the picture chosen in the build dialog.
    std::string loadingImage = settings.LoadingImage.empty() ? "" : PathJoin(dataDir, settings.LoadingImage);
    engine.LoadingScreen = [loadingImage](Engine&, Rect, float, const std::string&) { Glue3D::DrawGameLoadingScreen(loadingImage); };
    engine.Run(settings.Title, [&](Engine& e) {
        Glue3D::Project::Create(e, AssetManifest::Load(e.AssetDir), e.AssetDir, InitialLists(e.AssetDir));
        auto& stage = static_cast<Glue3D::Stage&>(*e.Stage);
        try
        {
            Glue3D::ProjectFiles::Load(dataDir, stage);
        }
        catch (const std::exception& ex)
        {
            std::fprintf(stderr, "could not load the game: %s\n", ex.what());
        }

        // Glue3D's own "exported game" mode: fullscreen game view, no editor, and the game
        // starts by itself (the green flag runs Glue3D's start-up, which runs the game).
        stage.allExportState = 1;
        stage.guiSplash = 0;
        stage.guiFreeze = 0;
        stage.errorPopup = 0;
        stage.allRun = 0;
        // The Glue3D logo animation: Glue3D's start-up plays it in an exported game unless its intro setting
        // (engine data item 212) is 0, so the build setting goes there.
        stage.ramEngineData.Replace(212, settings.Intro ? 1 : 0);
        stage.allSplashscreen = settings.Intro ? 0 : 7;

        // Mouse controls of game.movement (lock the mouse on click, Esc frees it).
        for (SpriteBase* s : e.Sprites)
            if (auto* glue = dynamic_cast<Glue3D::Glue3DSprite*>(s))
            {
                e.BeforeScripts.push_back([glue] { glue->GameControlsUpdate(); });
                e.Overlay.push_back([glue](Rect r) { glue->GameControlsDraw(r); });
                // Automated tests only (SCRATCH_AUTOMATION): run script lines on an object, list the objects.
                auto& hooks = Automation::Hooks();
                hooks["runscript"] = [glue](const std::string& arg) {
                    std::vector<std::string> parts;
                    std::stringstream ss(arg);
                    for (std::string p; std::getline(ss, p, '|');)
                    {
                        size_t a = p.find_first_not_of(' '), b = p.find_last_not_of(' ');
                        parts.push_back(a == std::string::npos ? "" : p.substr(a, b - a + 1));
                    }
                    if (parts.size() >= 2) glue->TestRunScript(parts[0], std::vector<std::string>(parts.begin() + 1, parts.end()));
                };
                hooks["objects"] = [glue](const std::string&) {
                    std::string out;
                    for (auto& v : glue->stage.scene.Items)
                        if (v.Str().ToUtf8().size() > 6 && v.Str().ToUtf8().ends_with(".start")) out += v.Str().ToUtf8() + " ";
                    std::printf("[objects] %s\n", out.c_str());
                    std::fflush(stdout);
                };
                e.BeforeScripts.push_back([glue] { glue->TestStep(); });
            }
    });
    return 0;
}
