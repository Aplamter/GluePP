// The editor's hand-written features (start menu, saving, build dialog, scene gizmo) aren't part of a game.
// These empty stand-ins let the shared Glue3D code compile without them (Custom/MainMenu.cpp, SceneGizmo.cpp
// and BuildGame.cpp are not compiled into the player).
#include "Glue3DSprite.g.h"

namespace Glue3D {

void Glue3DSprite::MainMenuFrame() {}
void Glue3DSprite::LoadProject(const std::string&) {}
void Glue3DSprite::SaveProject() {}
void Glue3DSprite::SaveShortcutUpdate() {}
void Glue3DSprite::Notify(const Value& message, const Value& icon, const Value& sound, const Value& dismissable)
{
    Message(message, icon, sound, dismissable);
}
void Glue3DSprite::OpenBuildDialog() {}
void Glue3DSprite::BuildDialogFrame() {}
bool Glue3DSprite::SceneViewActive() { return false; }
bool Glue3DSprite::InSceneViewport(double, double) { return false; }
void Glue3DSprite::SceneGizmoUpdate() {}
void Glue3DSprite::SceneGizmoDraw(Rect) {}
std::string Glue3DSprite::InspectedObject() { return ""; }
void Glue3DSprite::SelectForTest(const std::string&) {}

} // namespace Glue3D
