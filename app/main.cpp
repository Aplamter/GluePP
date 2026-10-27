// The Glue3D editor.
#include <cstdio>
#include <sstream>
#include <vector>

#include "Automation.h"
#include "Engine.h"
#include "Glue3DSprite.g.h"
#include "LoadingScreen.h"
#include "Project.g.h"

using namespace scratch;

int main()
{
    Engine engine;
    engine.LoadingScreen = Glue3D::DrawLoadingScreen;
    engine.Run(Glue3D::Project::Title, [](Engine& e) {
        AssetManifest manifest = AssetManifest::Load(e.AssetDir);
        Glue3D::Project::Create(e, manifest, e.AssetDir, InitialLists(e.AssetDir));

        // Hand-written editor additions (Custom/): scene toolbar + gizmos, Ctrl+S.
        Glue3D::Glue3DSprite* glue = nullptr;
        for (SpriteBase* s : e.Sprites)
            if ((glue = dynamic_cast<Glue3D::Glue3DSprite*>(s))) break;
        if (!glue) return;
        e.BeforeScripts.push_back([glue] { glue->SceneGizmoUpdate(); });
        e.Overlay.push_back([glue](Rect r) { glue->SceneGizmoDraw(r); });
        e.BeforeScripts.push_back([glue] { glue->SaveShortcutUpdate(); }); // Ctrl+S saves the open project
        e.BeforeScripts.push_back([glue] { glue->GameControlsUpdate(); }); // mouse controls of game.movement
        e.BeforeScripts.push_back([glue] { glue->ScrollWheelUpdate(); });  // the mouse wheel scrolls scrollbars
        e.Overlay.push_back([glue](Rect r) { glue->GameControlsDraw(r); });

        // Extra commands for automated tests (SCRATCH_AUTOMATION).
        auto& hooks = Automation::Hooks();
        hooks["state"] = [glue](const std::string&) {
            std::printf("[state] menu %s project %s folder %s\n", glue->MainMenuActive ? "True" : "False", glue->ProjectName.c_str(),
                        glue->ProjectFolder.c_str());
            std::fflush(stdout);
        };
        hooks["addobject"] = [glue](const std::string& arg) {
            std::istringstream a(arg);
            std::string name;
            double x = 0, y = 0, z = 0;
            a >> name >> x >> y >> z;
            glue->AddTestObject(name, x, y, z);
        };
        hooks["addcamera"] = [glue](const std::string& arg) {
            std::istringstream a(arg);
            std::string name;
            double x = 0, y = 0, z = 0;
            a >> name >> x >> y >> z;
            glue->AddTestCamera(name, x, y, z);
        };
        hooks["camera"] = [glue](const std::string& arg) {
            std::istringstream a(arg);
            double v[5] = {0, 0, 0, 0, 0};
            for (double& d : v) a >> d;
            glue->SetCamera(v[0], v[1], v[2], v[3], v[4]);
        };
        hooks["where"] = [glue](const std::string& name) {
            std::printf("[where] %s\n", glue->DescribeObject(name).c_str());
            std::fflush(stdout);
        };
        // gamescript <object> | <line> | <line>...  runs Glue3D script lines on an object once;
        // gameloop does it every frame (gameloop off stops). Used to test game.movement.
        auto runLines = [glue](const std::string& arg) {
            std::vector<std::string> parts;
            std::stringstream ss(arg);
            for (std::string p; std::getline(ss, p, '|');)
            {
                size_t a = p.find_first_not_of(' '), b = p.find_last_not_of(' ');
                parts.push_back(a == std::string::npos ? "" : p.substr(a, b - a + 1));
            }
            if (parts.size() < 2) return;
            for (size_t i = 1; i < parts.size(); i++)
            {
                glue->stage.scriptObject = Value(String::FromUtf8(parts[0]));
                glue->DecodeIDSeperatorIncludeSeperatorSpecialdot(Value(String::FromUtf8(parts[i] + " ")), Value(), u" (),"_s, Value(), u"."_s);
                String kind = glue->stage.decodedElements.Item(1).Str();
                if (kind == u"game") glue->ProccessGame();
                else if (kind == u"physics") glue->ProccessPhysics();
                else if (kind == u"object") glue->ProccessObject();
            }
        };
        hooks["gamescript"] = runLines;
        // runscript <object> | <line> | <line>...  runs a whole script on an object through Glue3D's interpreter
        // (a few dozen lines per frame), without starting the game.
        hooks["runscript"] = [glue](const std::string& arg) {
            std::vector<std::string> parts;
            std::stringstream ss(arg);
            for (std::string p; std::getline(ss, p, '|');)
            {
                size_t a = p.find_first_not_of(' '), b = p.find_last_not_of(' ');
                parts.push_back(a == std::string::npos ? "" : p.substr(a, b - a + 1));
            }
            if (parts.size() < 2) return;
            glue->TestRunScript(parts[0], std::vector<std::string>(parts.begin() + 1, parts.end()));
        };
        e.BeforeScripts.push_back([glue] { glue->TestStep(); });
        static std::string loopArg;
        hooks["gameloop"] = [](const std::string& arg) { loopArg = arg == "off" ? "" : arg; };
        e.BeforeScripts.push_back([runLines] { if (!loopArg.empty()) runLines(loopArg); });
        hooks["ram"] = [glue](const std::string& arg) { // ram <address>...: Glue3D's engine data at those addresses
            std::istringstream a(arg);
            std::string out;
            for (int addr; a >> addr;) out += std::to_string(addr) + "=" + glue->stage.ramEngineData.Item(addr * 2).Str().ToUtf8() + " ";
            std::printf("[ram] %s\n", out.c_str());
            std::fflush(stdout);
        };
        hooks["select"] = [glue](const std::string& name) { glue->SelectForTest(name); };
        hooks["console"] = [glue](const std::string& text) { glue->SubmitConsole(text); };
        hooks["rungame"] = [glue](const std::string&) { glue->Glue3dRun(); };
        hooks["stopgame"] = [glue](const std::string&) { glue->Glue3dStop(); };
        hooks["load"] = [glue](const std::string& folder) { glue->LoadProject(folder); };
        hooks["dump"] = [glue](const std::string& field) {
            for (auto& [name, member] : Glue3D::Stage::ValueFields())
                if (field == name) std::printf("[dump] %s = %s\n", name, (glue->stage.*member).Str().ToUtf8().c_str());
            for (auto& [name, member] : Glue3D::Stage::ListFields())
                if (field == name)
                {
                    std::string s;
                    for (auto& v : (glue->stage.*member).Items) s += (s.empty() ? "" : " | ") + v.Str().ToUtf8();
                    std::printf("[dump] %s = %s\n", name, s.c_str());
                }
            std::fflush(stdout);
        };
    });
    return 0;
}
