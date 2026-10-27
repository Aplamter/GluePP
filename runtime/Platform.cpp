#include "Platform.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

namespace scratch {

std::filesystem::path FsPath(const std::string& utf8)
{
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

std::string PathText(const std::filesystem::path& p)
{
    auto u8 = p.generic_u8string();
    return std::string(u8.begin(), u8.end());
}

std::string PathJoin(const std::string& a, const std::string& b)
{
    if (a.empty()) return b;
    char last = a.back();
    return (last == '/' || last == '\\') ? a + b : a + "/" + b;
}

std::string ExePath()
{
#ifdef _WIN32
    std::wstring buf(MAX_PATH, L'\0');
    for (;;)
    {
        DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        if (n < buf.size()) { buf.resize(n); break; }
        buf.resize(buf.size() * 2);
    }
    return PathText(std::filesystem::path(buf));
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof buf;
    if (_NSGetExecutablePath(buf, &size) == 0) return PathText(std::filesystem::weakly_canonical(buf));
    return ".";
#else
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) return ".";
    buf[n] = 0;
    return buf;
#endif
}

std::string ExeDir() { return PathText(FsPath(ExePath()).parent_path()); }

bool ReadFile(const std::string& path, std::string& out)
{
    std::ifstream in(FsPath(path), std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool WriteFileAtomic(const std::string& path, const std::string& data)
{
    auto target = FsPath(path);
    auto temp = target;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(data.data(), (std::streamsize)data.size());
        if (!out) return false;
    }
    std::error_code ec;
    std::filesystem::rename(temp, target, ec);
    if (ec)
    {
        std::filesystem::remove(target, ec);
        std::filesystem::rename(temp, target, ec);
    }
    return !ec;
}

void OpenInFileBrowser(const std::string& folder)
{
#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", FsPath(folder).wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::string cmd = "open \"" + folder + "\" &";
    if (std::system(cmd.c_str()) != 0) { /* nothing to open it with */ }
#else
    std::string cmd = "xdg-open \"" + folder + "\" >/dev/null 2>&1 &";
    if (std::system(cmd.c_str()) != 0) { /* nothing to open it with */ }
#endif
}

std::string PickImageFile(const std::string& title)
{
    if (const char* test = std::getenv("SCRATCH_PICK_FILE")) return test;
#ifdef _WIN32
    wchar_t file[4096] = L"";
    std::wstring wtitle = FsPath(title).wstring();
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = L"Pictures (*.png, *.jpg, *.jpeg)\0*.png;*.jpg;*.jpeg\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 4096;
    ofn.lpstrTitle = wtitle.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return "";
    return PathText(std::filesystem::path(file));
#else
#ifdef __APPLE__
    std::string cmd = "osascript -e 'POSIX path of (choose file of type {\"png\", \"jpg\", \"jpeg\"} with prompt \"" + title + "\")' 2>/dev/null";
#else
    std::string cmd = "zenity --file-selection --title=\"" + title + "\" --file-filter='Pictures | *.png *.jpg *.jpeg *.PNG *.JPG *.JPEG' 2>/dev/null"
                      " || kdialog --getopenfilename ~ 'Pictures (*.png *.jpg *.jpeg)' 2>/dev/null";
#endif
    std::string out;
    if (FILE* p = popen(cmd.c_str(), "r"))
    {
        char buf[4096];
        while (fgets(buf, sizeof buf, p)) out += buf;
        pclose(p);
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
#endif
}

std::vector<std::string> DriveRoots()
{
    std::vector<std::string> out;
#ifdef _WIN32
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; i++)
        if (mask & (1u << i))
        {
            std::string root = std::string(1, (char)('A' + i)) + ":/";
            UINT type = GetDriveTypeA(root.c_str());
            if (type != DRIVE_NO_ROOT_DIR) out.push_back(root);
        }
#else
    out.push_back("/");
#endif
    return out;
}

} // namespace scratch
