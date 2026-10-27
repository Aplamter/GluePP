// Hand-written: the loading screen shown while Glue3D starts (editor and built games).
#pragma once
#include <string>

#include "Engine.h"

namespace Glue3D {

/// Draws one frame of the loading screen. Set it as Engine::LoadingScreen before Engine::Run.
void DrawLoadingScreen(scratch::Engine& engine, scratch::Rect stage, float progress, const std::string& what);

/// A built game's loading screen: black, or the given picture (fitted to the window, centered).
void DrawGameLoadingScreen(const std::string& imagePath);

} // namespace Glue3D
