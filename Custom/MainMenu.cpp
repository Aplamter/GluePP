// Hand-written (not overwritten by the converter): Glue3D's start menu.
//
// After the welcome popup, the app shows this menu instead of going straight into the editor:
//   - "create a new project": name it, pick the engine (tile / segment / mode7), and it is created in
//     the Projects folder next to the Glue3D program. The engine no longer has to be configured in settings.
//   - "load a project": browse to a project folder and open it.
// Everything is drawn with Glue3D's own GUI blocks (boxes, buttons, font, pointer, tooltips, sounds),
// so it follows the selected theme like the rest of the editor.
#include <algorithm>
#include <cstdio>
#include <filesystem>

#include "Engine.h"
#include "Glue3DSprite.g.h"
#include "ProjectFiles.h"

namespace Glue3D {

namespace {

/// Text for Glue3D's blocks.
Value T(const std::string& s) { return Value(String::FromUtf8(s)); }
/// An empty argument ("" in Scratch).
const Value E;

std::string Lower(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    return s;
}

std::string Trim(const std::string& s, const char* chars)
{
    size_t a = s.find_first_not_of(chars), b = s.find_last_not_of(chars);
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// Address in Glue3D's engine data that the name field types into (the same one Glue3D's
// "new object" dialog uses; typed text goes there automatically while inputAddress points at it).
constexpr int NameAddress = 12;
constexpr int NameLimit = 24;
constexpr int BrowseRows = 7;
constexpr int ActionCreate = 1, ActionLoad = 2;

} // namespace

// ================= one frame of the menu (called from Glue3DAll instead of the editor) =================

void Glue3DSprite::MainMenuFrame()
{
    // The same per-frame bookkeeping Glue3D's editor does in "Main: control".
    stage.inputAddress = (menuDialog_ == MenuDialog::Create && Eq(stage.guiFreeze, 0) && Eq(stage.guiSplash, 0)) ? NameAddress : 0;
    stage.inputLimit = NameLimit;
    stage.guiPointerID = u""_s;
    stage.guiInfo = u""_s;
    if (!MouseDown())
    {
        stage.dragTime = 0;
        stage.guiSlider = 0;
    }
    stage.guiPointerX = stage.guiPointerX.Num() + Nz((MouseX() - stage.guiPointerX.Num()) / (1 + 5 * stage.timeDelta.Num()));
    stage.guiPointerY = stage.guiPointerY.Num() + Nz((MouseY() - stage.guiPointerY.Num()) / (1 + 5 * stage.timeDelta.Num()));
    stage.guiFocusEffect = stage.guiFocusEffect.Num() - stage.timeDelta.Num();
    clickTaken_ = false;

    PenClear();
    DrawMenuBackground();
    DrawMenuButtons();
    DrawBannerCarousel();

    if (menuDialog_ == MenuDialog::Create) DrawCreateDialog();
    else if (menuDialog_ == MenuDialog::Load) DrawLoadDialog();

    if (Eq(stage.guiSplash, 1)) DrawWelcomePopup();
    if (Eq(stage.errorPopup, 1)) ElementError(); // Glue3D's message popup

    // Tooltips, exactly as the editor draws them.
    if (Eq(stage.guiInfo, 1)) stage.guiInfoEffect = stage.guiInfoEffect.Num() + 0.15;
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_gui.info"_s, stage.guiInfoX, stage.guiInfoY, 90, 35,
                                                      (1 - stage.guiInfoEffect.Num()) * 100, -50, 0, E, E, 1, E, E);
    double len = stage.guiInfoName.Str().Length();
    TextOnScaleCenterGbcCursorLimitScroll(stage.guiInfoName, stage.guiInfoX, stage.guiInfoY, 0.67 + Num(Gt(len, 4)) * ((len - 5) * -0.015), 1, 0,
                                          (1 - stage.guiInfoEffect.Num()) * 135, 100, 0, E, 1, E, E);
    stage.guiInfoEffect = stage.guiInfoEffect.Num() * 0.9;

    MainAfterControl(); // pointer, click bookkeeping, theme contrast
}

// ================= small helpers =================

bool Glue3DSprite::MenuInputFree() { return Eq(stage.guiFreeze, 0) && Eq(stage.guiSplash, 0) && !Eq(stage.errorPopup, 1); }

bool Glue3DSprite::Hovering(double x1, double y1, double x2, double y2)
{
    return MenuInputFree() && MouseX() >= x1 && MouseX() <= x2 && MouseY() <= y1 && MouseY() >= y2;
}

/// A fresh click this frame (same rule Glue3D's buttons use), given to one control only.
bool Glue3DSprite::TakeClick()
{
    if (clickTaken_ || !MenuInputFree() || !MouseDown() || !Eq(stage.guiPrevMouseState, 0)) return false;
    clickTaken_ = true;
    StartSound(u"Windows Vista Beta Balloon"_s);
    return true;
}

/// Glue3D text (its own font). Dark text by default, like dialog text.
void Glue3DSprite::Label(const std::string& text, double x, double y, double scale, bool center, double ghost, double brightness, int limit)
{
    TextOnScaleCenterGbcCursorLimitScroll(T(text), x, y, scale, center ? 1 : 0, E, ghost, brightness, 0, E, E,
                                          limit > 0 ? Value(limit) : E, E);
}

/// Small grey "info" text, like the hints in Glue3D's dialogs.
void Glue3DSprite::InfoText(const std::string& text, double x, double y, double scale)
{
    TextOnScaleCenterGbcCursorLimitScroll(T(text), x, y, scale, E, E, 25, 30, -kInfinity, E, E, E, E);
}

// ================= background, logo, buttons =================

void Glue3DSprite::DrawMenuBackground()
{
    double t = stage.timeTimer.Num();
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_gui.contrast"_s, 0, 0, 90, 100, 0, -100, 0, E, E, E, E, E);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_engine.clouds"_s, 0, 0, 90 + 0.6 * Nz(Sin(t * 6)), 130, 0, -15, 0, E, E, E, E, E);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_gui.darkfade"_s, 0, -120, 90, 100, 20, 0, 0, E, E, E, E, E);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_engine.beam"_s, 0, 112, 90, 125, 55, 0, 0, E, E, E, E, E);
    // The Glue3D logo, laid out like the start-up animation.
    double bob = 1.5 * Nz(Sin(t * 90));
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_engine.g"_s, -48, 122 + bob, 90, 24, 0, 0, 0, E, E, E, 1, E);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_engine.l"_s, -14, 118 + bob, 90, 24, 0, 0, 0, E, E, E, 1, E);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_engine.u"_s, 14, 118 + bob, 90, 24, 0, 0, 0, E, E, E, 1, E);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_engine.e3d"_s, 50, 110 + bob, 90, 26, 0, 0, 0, E, E, E, 1, E);

    std::string root = Lower(ProjectFiles::ProjectsRoot());
    TextOnScaleCenterGbcCursorLimitScroll(T("projects folder: " + root), -232, -168, 0.4, E, E, 45, 100, 0, E, E, 70, E);
}

void Glue3DSprite::DrawMenuButtons()
{
    MenuButton(-215, 62, -25, 17, "_icon.filenew", "create a new project", "name it and pick the engine", ActionCreate);
    MenuButton(-215, 5, -25, -40, "_icon.filemove", "load a project", "open a saved project folder", ActionLoad);
}

void Glue3DSprite::MenuButton(double x1, double y1, double x2, double y2, const char* icon, const char* title, const char* subtitle, int action)
{
    bool hover = menuDialog_ == MenuDialog::None && Hovering(x1, y1, x2, y2);
    double mid = (y1 + y2) / 2;
    RenderBoxToBoxTypeBorderAlternate(x1, y1, x2, y2, 1, 2, hover ? 1 : 0, E, E);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(T(icon), x1 + 24, mid + 1, 90, hover ? 78 : 72, 5, hover ? 10 : 0, 0, E, E, E, 1, E);
    Label(title, x1 + 46, mid + 8, 0.66, false, 35);
    InfoText(subtitle, x1 + 46, mid - 11, 0.43);
    if (!hover) return;
    stage.guiPointerID = u"click"_s;
    if (!TakeClick()) return;
    if (action == ActionCreate)
    {
        EditAddress(NameAddress, E);
        menuDialog_ = MenuDialog::Create;
    }
    else
    {
        OpenBrowser(ProjectFiles::ProjectsRoot());
        menuDialog_ = MenuDialog::Load;
    }
}

/// Glue3D's feature banners, cycling on the right.
void Glue3DSprite::DrawBannerCarousel()
{
    double t = stage.timeTimer.Num();
    int index = (int)std::floor(t / 4) % 6 + 1;
    double phase = std::fmod(t, 4);
    double fade = phase < 0.4 ? (1 - phase / 0.4) * 100 : phase > 3.6 ? (phase - 3.6) / 0.4 * 100 : 0;
    RenderBoxToBoxTypeBorderAlternate(22, 72, 208, -132, 1, 2, 0, E, E);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(T("_engine.banner_" + std::to_string(index)), 115, -30, 90, 51, fade, 0, 0, E, E, E, 1, E);
}

void Glue3DSprite::DrawWelcomePopup()
{
    // Glue3D's original welcome popup (from "Editor: screen"), now shown over the start menu.
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_engine.splash"_s, 0, 0, 90, 81, 50 + Nz(5 / Timer()), -100, 0, E, E, E, E, E);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_engine.splash"_s, 0, 0, 90, 80, 5 / Timer(), 0, 0, E, E, E, E, E);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_button.proceed"_s, 155, -100, 90, 60, 25, 0, 0, 1, E, E, 1, u"let's go!"_s);
    if (Eq(stage.guiClick, 1)) stage.guiSplash = 0;
}

// ================= "create a new project" =================

std::string Glue3DSprite::EngineString() const
{
    std::string code = std::string(engineTile_ ? "t" : "") + (engineSegment_ ? "s" : "") + (engineMode7_ ? "m" : "");
    if (code == "t") return "2dray: tile,t,";
    if (code == "ts") return "2dray: tile+seg,ts,";
    if (code == "tm") return "2dray: tile+mode7,tm,";
    if (code == "tsm") return "2dray: tile+seg+m7,tsm,";
    if (code == "s") return "2dray: segment,s,";
    if (code == "sm") return "2dray: seg+mode7,sm,";
    if (code == "m") return "2dray: mode7,m,";
    return "";
}

void Glue3DSprite::DrawCreateDialog()
{
    double live = MenuInputFree() ? 1 : 0;
    // Same layout as Glue3D's "new object" dialog.
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_gui.dark"_s, 0, 0, 90, 100, 0, 0, 0, 0, E, E, 0, E);
    RenderBoxToBoxTypeBorderAlternate(-95, 85, 95, -135, 1, 2, Num(Gt(stage.guiFocusEffect, 0)) * 1, E, E);
    Label("new project", -90, 78, 0.55, false, 75);

    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_button.delete"_s, 85, 75, 90, 38, 30, 0, 0, live, E, E, 1, E);
    if (Eq(stage.guiClick, 1)) { menuDialog_ = MenuDialog::None; return; }

    Label("name your project", 0, 55, 0.67, true);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_icon.filenew"_s, -62, 28, 90, 70, 10, 0, 0, live, E, E, 1, u"project"_s);
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_gui.info"_s, 14, 28, 90, 35, 50, -100, 0, E, E, E, 0, E);
    String name = stage.ramEngineData.Item(NameAddress * 2).Str();
    TextOnScaleCenterGbcCursorLimitScroll(name, -28, 27.5, 0.75 + Num(Gt(name.Length(), 6)) * ((name.Length() - 7) * -0.022), E, E, 25, 30,
                                          -kInfinity, 1, E, E, E);

    Label("choose the engine", 0, 0, 0.65, true);
    EngineToggle("_engine.tile", "tile", -58, engineTile_);
    EngineToggle("_engine.segment", "segment", 0, engineSegment_);
    EngineToggle("_engine.mode7", "mode7", 58, engineMode7_);
    std::string engine = EngineString();
    InfoText(engine.empty() ? "pick at least one" : engine.substr(0, engine.find(',')), engine.empty() ? -32 : -48, -84, 0.48);

    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_button.info"_s, -85, -118, 90, 28, 40, 0, 0, live, E, E, 1, E);
    InfoText("saved in the projects folder", -74, -112, 0.42);
    InfoText("next to glue3d.exe", -74, -123, 0.42);

    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_button.accept"_s, 70, -112, 90, 45, 5, -10, 0, live, E, E, 1, u"create"_s);
    if (Eq(stage.guiClick, 1) || (Eq(stage.inputSubmit, 1) && MenuInputFree()))
    {
        stage.inputSubmit = 0;
        TryCreateProject();
    }
}

void Glue3DSprite::EngineToggle(const char* costume, const char* label, double x, bool& on)
{
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(T(costume), x, -35, 90, on ? 26 : 20, on ? 10 : 70, 0, 0, MenuInputFree() ? 1 : 0, E, E,
                                                      on ? 1 : 0, T(label));
    if (Eq(stage.guiClick, 1)) on = !on;
    Label(label, x, -66, 0.52, true, on ? 30 : 75);
}

void Glue3DSprite::TryCreateProject()
{
    std::string name = Trim(Trim(stage.ramEngineData.Item(NameAddress * 2).Str().ToUtf8(), " \t\r\n"), ".");
    std::string engine = EngineString();
    if (name.empty() || name == "0") { Notify(u"please name your project"_s, u"_icon.warn"_s, u"Error_1"_s, 1); return; }
    if (engine.empty()) { Notify(u"pick at least one engine part"_s, u"_icon.configure"_s, u"Error_1"_s, 1); return; }
    if (name.find_first_of("/\\:*?\"<>|") != std::string::npos)
    {
        Notify(u"a name can't have / \\ : * ? < > |"_s, u"_icon.warn"_s, u"Error_1"_s, 1);
        return;
    }
    std::string folder = scratch::PathJoin(ProjectFiles::ProjectsRoot(), name);
    std::error_code ec;
    if (std::filesystem::exists(scratch::FsPath(folder), ec)) { Notify(u"that project already exists"_s, u"_icon.warn"_s, u"Error_1"_s, 1); return; }

    // Glue3D's own "format": a fresh, empty project.
    StartEngine(1);
    stage.ramEngineData.Replace(300, u"none"_s);
    stage.guiSplash = 0;
    // The engine choice, exactly what "settings > configure engine" does.
    EditAddress(36, T(engine));
    DecodeIDSeperatorIncludeSeperatorSpecialdot(stage.ramEngineData.Item(72), E, E, E, E);
    stage.engineType = stage.decodedElements.Item(2);
    EditAddress(38, u"configured"_s);

    ProjectFolder = folder;
    ProjectName = name;
    try
    {
        ProjectFiles::Save(folder, name, engine, stage);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "%s\n", e.what());
        Notify(u"could not create the project folder"_s, u"_icon.warn"_s, u"Error_3"_s, E);
        return;
    }
    EnterEditor();
    Notify(u"project created"_s, u"_icon.filenew"_s, u"Windows Vista Beta Contact Online"_s, 1);
}

// ================= "load a project" =================

void Glue3DSprite::OpenBrowser(const std::string& folder)
{
    namespace fs = std::filesystem;
    browseFolder_ = folder;
    browseScroll_ = 0;
    browseEntries_.clear();
    if (folder.empty())
    {
        for (const std::string& d : scratch::DriveRoots()) browseEntries_.push_back({d, d, "drive", false, false});
        return;
    }
    std::error_code ec;
    fs::path p = scratch::FsPath(folder);
    fs::path parent = p.has_relative_path() ? p.parent_path() : fs::path();
    if (p.has_relative_path() && !parent.empty() && parent != p) browseEntries_.push_back({scratch::PathText(parent), "..", "back", false, true});
#ifdef _WIN32
    else browseEntries_.push_back({"", "..", "drives", false, true});
#endif

    std::vector<std::pair<std::string, std::string>> dirs; // (path, name)
    for (fs::directory_iterator it(p, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec))
    {
        std::error_code e2;
        if (!it->is_directory(e2)) continue;
        std::string name = scratch::PathText(it->path().filename());
        if (name.empty() || name[0] == '.' || name[0] == '$' || name == "System Volume Information") continue;
        dirs.push_back({scratch::PathText(it->path()), name});
    }
    if (ec && dirs.empty() && browseEntries_.empty())
    {
        Notify(u"can't open that folder"_s, u"_icon.warn"_s, u"Error_1"_s, 1);
        return;
    }
    std::sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b) { return Lower(a.second) < Lower(b.second); });
    for (auto& [path, name] : dirs)
    {
        std::optional<ProjectFiles::Info> info;
        if (ProjectFiles::IsProjectFolder(path)) info = ProjectFiles::ReadInfo(path);
        std::string detail;
        if (info)
        {
            std::string eng = info->Engine.substr(0, info->Engine.find(','));
            if (eng.rfind("2dray: ", 0) == 0) eng = eng.substr(7);
            detail = eng + "  " + info->Saved.substr(0, 10);
        }
        browseEntries_.push_back({path, name, detail, info.has_value(), false});
    }
}

void Glue3DSprite::DrawLoadDialog()
{
    double live = MenuInputFree() ? 1 : 0;
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_gui.dark"_s, 0, 0, 90, 100, 0, 0, 0, 0, E, E, 0, E);
    RenderBoxToBoxTypeBorderAlternate(-125, 100, 125, -145, 1, 2, Num(Gt(stage.guiFocusEffect, 0)) * 1, E, E);
    Label("load project", -120, 93, 0.55, false, 75);

    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_button.delete"_s, 115, 90, 90, 38, 30, 0, 0, live, E, E, 1, E);
    if (Eq(stage.guiClick, 1)) { menuDialog_ = MenuDialog::None; return; }

    Label("pick the project folder", 0, 72, 0.62, true);
    std::string where = browseFolder_.empty() ? "this computer" : Lower(browseFolder_);
    if (where.size() > 52) where = "..." + where.substr(where.size() - 49);
    InfoText(where, -117, 57, 0.42);

    // Mouse wheel scrolls (Glue3D turns the wheel into guiScroll).
    int count = (int)browseEntries_.size();
    int maxScroll = std::max(0, count - BrowseRows);
    if (MenuInputFree() && stage.guiScroll.Num() != 0)
        browseScroll_ = std::clamp(browseScroll_ - (int)stage.guiScroll.Num(), 0, maxScroll);

    for (int row = 0; row < BrowseRows; row++)
    {
        int i = browseScroll_ + row;
        if (i >= count) break;
        BrowseEntry e = browseEntries_[i];
        double y = 38 - row * 22;
        bool hover = Hovering(-117, y + 10, 117, y - 10);
        if (hover) RenderBoxToBoxTypeBorderAlternate(-117, y + 10, 117, y - 10, 1, 2, 1, E, E);
        const char* icon = e.IsUp ? "_icon.filemove" : e.IsProject ? "_icon.scene" : "_icon.file";
        Render2DOnDirSizeGbcInteractiveShadowHoverMessage(T(icon), -103, y, 90, e.IsProject ? 48 : 42, 10, 0, 0, E, E, E, 1, E);
        std::string label = Lower(e.Label);
        Label(label.size() > 26 ? label.substr(0, 25) + "~" : label, -88, y + 1, 0.56, false, e.IsProject ? 25 : 45);
        if (!e.Detail.empty()) InfoText(Lower(e.Detail), 28, y + 1, 0.42);
        if (hover)
        {
            stage.guiPointerID = u"click"_s;
            if (e.IsProject)
            {
                stage.guiInfo = 1;
                stage.guiInfoName = u"open"_s;
                stage.guiInfoX = MouseX();
                stage.guiInfoY = stage.guiInfoY.Num() + Nz((y + 22 - stage.guiInfoY.Num()) / 2);
            }
            if (TakeClick())
            {
                if (e.IsProject) LoadProject(e.Path);
                else OpenBrowser(e.Path);
                return;
            }
        }
    }
    if (count == 0 || (count == 1 && browseEntries_[0].IsUp)) InfoText("this folder is empty", -40, 0, 0.5);

    // Footer: shortcut back to the projects folder, and a scroll hint.
    Render2DOnDirSizeGbcInteractiveShadowHoverMessage(u"_button.refresh"_s, -108, -130, 90, 30, 30, 0, 0, live, E, E, 1, u"projects folder"_s);
    if (Eq(stage.guiClick, 1)) OpenBrowser(ProjectFiles::ProjectsRoot());
    std::string hint = maxScroll > 0 ? "scroll for more  (" + std::to_string(browseScroll_ + 1) + "-" +
                                           std::to_string(std::min(count, browseScroll_ + BrowseRows)) + " of " + std::to_string(count) + ")"
                                     : "click a project to open it";
    InfoText(hint, -94, -129, 0.42);
}

void Glue3DSprite::LoadProject(const std::string& folder)
{
    ProjectFiles::Info info;
    try
    {
        info = ProjectFiles::Load(folder, stage);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "%s\n", e.what());
        Notify(u"this project could not be loaded"_s, u"_icon.warn"_s, u"Error_3"_s, E);
        return;
    }
    // Like reopening a saved project in Scratch: Glue3D rebuilds the editor from the saved data.
    stage.guiSplash = 0;
    StartEngine(0);
    ProjectFolder = folder;
    ProjectName = info.Name;
    EnterEditor();
    Notify(T("loaded " + info.Name), u"_icon.filemove"_s, u"Exclame_2"_s, 1);
}

/// Leaves the menu and shows the editor, with all temporary GUI state cleared.
void Glue3DSprite::EnterEditor()
{
    stage.guiSplash = 0;
    stage.guiFreeze = 0;
    stage.errorPopup = 0;
    stage.inputAddress = 0;
    stage.inputSubmit = 0;
    stage.menuSelection = u""_s;
    stage.editorSceneTool = u""_s;
    stage.editorSceneSubject = u""_s;
    stage.dragId = u""_s;
    stage.timeTimer = Timer(); // no time jump on the first editor frame
    EditAddress(13, 0);        // no dropdown/menu open
    selected_.clear();         // gizmo selection
    menuDialog_ = MenuDialog::None;
    MainMenuActive = false;
    GetEngine().SetWindowTitle("Glue3D - " + ProjectName);
}

void Glue3DSprite::Notify(const Value& message, const Value& icon, const Value& sound, const Value& dismissable)
{
    stage.guiClickLimit = 0.5;
    Message(message, icon, sound, dismissable);
}

// ================= saving (File > save project.., or Ctrl+S) =================

void Glue3DSprite::SaveProject()
{
    if (LevelActive())
    {
        // the game is playing another level (game.load_scene): saving now would save that level here
        Notify(u"stop the game before saving"_s, u"_icon.warn"_s, u"Error_1"_s, 1);
        return;
    }
    if (ProjectFolder.empty() || ProjectName.empty())
    {
        Notify(u"no project is open"_s, u"_icon.warn"_s, u"Error_1"_s, 1);
        return;
    }
    try
    {
        ProjectFiles::Save(ProjectFolder, ProjectName, stage.ramEngineData.Item(72).Str().ToUtf8(), stage);
        Notify(u"project saved"_s, u"_icon.file"_s, u"Windows XP Print complete"_s, 1);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "%s\n", e.what());
        Notify(u"the project could not be saved"_s, u"_icon.warn"_s, u"Error_3"_s, E);
    }
}

void Glue3DSprite::SaveShortcutUpdate()
{
    if (MainMenuActive) return;
    auto& in = GetEngine().Input;
    bool ctrl = in.IsPhysicalKeyDown(scratch::Key::LeftControl) || in.IsPhysicalKeyDown(scratch::Key::RightControl);
    if (ctrl && in.IsPhysicalKeyPressed(scratch::Key::S)) SaveProject();
}

} // namespace Glue3D
