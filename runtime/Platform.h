// Files and folders, with UTF-8 paths on every system (Windows included).
#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace scratch {

/// UTF-8 text -> filesystem path (correct for non-English folder names on Windows).
std::filesystem::path FsPath(const std::string& utf8);
/// Filesystem path -> UTF-8 text.
std::string PathText(const std::filesystem::path& p);
/// Joins path parts with '/' (Windows accepts it too).
std::string PathJoin(const std::string& a, const std::string& b);
/// The folder the running program is in.
std::string ExeDir();
/// The full path of the running program.
std::string ExePath();

bool ReadFile(const std::string& path, std::string& out);
/// Writes to a temporary file first, then replaces the target, so a crash never leaves half a file.
bool WriteFileAtomic(const std::string& path, const std::string& data);

/// Shows a folder in Explorer / Finder / the file manager.
void OpenInFileBrowser(const std::string& folder);
/// The system's "open file" window, for pictures (.png, .jpg, .jpeg). Returns the chosen file, or "" if cancelled.
/// (Testing: SCRATCH_PICK_FILE=<path> answers it without a window.)
std::string PickImageFile(const std::string& title);

/// Drive roots ("C:/", "D:/") on Windows; "/" elsewhere.
std::vector<std::string> DriveRoots();

} // namespace scratch
