// Hand-written (not overwritten by the converter): saving and loading Glue3D projects as folders.
//
// In Scratch, a Glue3D project *was* the state of all its variables and lists (saving the .sb3 saved them).
// So a project folder holds exactly that, as JSON, in "project.glue3d":
//
//   Projects/
//     my-game/
//       project.glue3d   <- name, engine, dates, and every variable and list of the project
//
// Projects saved by the C# version of Glue3D open here too (same file format).
#pragma once
#include <optional>
#include <string>

namespace Glue3D {

class Stage;

namespace ProjectFiles {

inline constexpr const char* FileName = "project.glue3d";

/// New projects are created here: a "Projects" folder next to the Glue3D program.
std::string ProjectsRoot();
bool IsProjectFolder(const std::string& folder);

/// Basic facts about a saved project, read without loading it.
struct Info
{
    std::string Name, Engine, Saved; // Saved: "yyyy-mm-ddThh:mm:ss" (or "")
};
std::optional<Info> ReadInfo(const std::string& folder);

/// Saves every variable and list of the stage. Throws std::runtime_error on failure.
void Save(const std::string& folder, const std::string& name, const std::string& engine, const Stage& stage);
/// Puts every saved variable and list back into the stage. Throws std::runtime_error on failure.
Info Load(const std::string& folder, Stage& stage);

} // namespace ProjectFiles
} // namespace Glue3D
