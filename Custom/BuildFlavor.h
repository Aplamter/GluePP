// Hand-written: tells the shared Glue3D code whether it is being compiled into the editor or into a game.
// The game player (Player/, built with GLUE3D_PLAYER defined) is the same code with IsEditor = false, so
// every `if (BuildFlavor::IsEditor ...)` branch is compiled out and the linker drops the editor's code.
#pragma once

namespace Glue3D::BuildFlavor {
#ifdef GLUE3D_PLAYER
inline constexpr bool IsEditor = false;
#else
inline constexpr bool IsEditor = true;
#endif
} // namespace Glue3D::BuildFlavor
