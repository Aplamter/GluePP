#include "ProjectFiles.h"

#include <ctime>
#include <filesystem>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "Platform.h"
#include "Stage.g.h"

namespace Glue3D::ProjectFiles {

using json = nlohmann::json;
using namespace scratch;

static constexpr const char* Format = "gluepp-project";
static constexpr const char* CSharpFormat = "glue3d-csharp-project"; // the C# version's projects load too
static constexpr int FormatVersion = 1;

std::string ProjectsRoot()
{
    std::string root = PathJoin(ExeDir(), "Projects");
    std::error_code ec;
    std::filesystem::create_directories(FsPath(root), ec);
    return root;
}

bool IsProjectFolder(const std::string& folder)
{
    std::error_code ec;
    return std::filesystem::is_regular_file(FsPath(PathJoin(folder, FileName)), ec);
}

static std::string NowText()
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

static std::string FolderName(const std::string& folder) { return PathText(FsPath(folder).filename()); }

static Info InfoFrom(const json& root, const std::string& folder)
{
    Info info;
    info.Name = root.contains("name") && root["name"].is_string() ? root["name"].get<std::string>() : FolderName(folder);
    info.Engine = root.contains("engine") && root["engine"].is_string() ? root["engine"].get<std::string>() : "";
    info.Saved = root.contains("saved") && root["saved"].is_string() ? root["saved"].get<std::string>() : "";
    return info;
}

std::optional<Info> ReadInfo(const std::string& folder)
{
    try
    {
        std::string text;
        if (!ReadFile(PathJoin(folder, FileName), text)) return std::nullopt;
        return InfoFrom(json::parse(text), folder);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

// ---------------- save ----------------

static json ToJson(const Value& v)
{
    switch (v.Type())
    {
    case ValueType::Boolean: return v.Bool();
    case ValueType::String: return v.RawString().ToUtf8();
    default:
    {
        double n = v.RawNumber();
        if (std::isfinite(n)) return n;
        return json{{"number", Js::NumberToUtf8(n)}}; // NaN / Infinity
    }
    }
}

void Save(const std::string& folder, const std::string& name, const std::string& engine, const Stage& stage)
{
    std::error_code ec;
    std::filesystem::create_directories(FsPath(folder), ec);
    std::string path = PathJoin(folder, FileName);
    std::string created = NowText();
    {
        std::string old;
        if (ReadFile(path, old))
        {
            try
            {
                auto j = json::parse(old);
                if (j.contains("created") && j["created"].is_string()) created = j["created"].get<std::string>();
            }
            catch (...) {}
        }
    }

    json root = json::object();
    root["format"] = Format;
    root["version"] = FormatVersion;
    root["name"] = name;
    root["engine"] = engine;
    root["created"] = created;
    root["saved"] = NowText();
    json vars = json::object();
    for (auto& [fieldName, member] : Stage::ValueFields()) vars[fieldName] = ToJson(stage.*member);
    root["variables"] = std::move(vars);
    json lists = json::object();
    for (auto& [fieldName, member] : Stage::ListFields())
    {
        json arr = json::array();
        for (const Value& v : (stage.*member).Items) arr.push_back(ToJson(v));
        lists[fieldName] = std::move(arr);
    }
    root["lists"] = std::move(lists);

    // Written to a temporary file first, so a crash mid-save never leaves a broken project.
    if (!WriteFileAtomic(path, root.dump(-1, ' ', false, json::error_handler_t::replace)))
        throw std::runtime_error("could not write " + path);
}

// ---------------- load ----------------

static Value FromJson(const json& e)
{
    if (e.is_number()) return Value(e.get<double>());
    if (e.is_boolean()) return Value(e.get<bool>());
    if (e.is_string()) return Value(String::FromUtf8(e.get_ref<const std::string&>()));
    if (e.is_object() && e.contains("number") && e["number"].is_string())
        return Value(Js::ToNumber(String::FromUtf8(e["number"].get<std::string>())));
    return Value();
}

Info Load(const std::string& folder, Stage& stage)
{
    std::string text;
    if (!ReadFile(PathJoin(folder, FileName), text)) throw std::runtime_error("no project.glue3d in that folder");
    json root = json::parse(text); // throws on broken files
    std::string fmt = root.contains("format") && root["format"].is_string() ? root["format"].get<std::string>() : "";
    if (fmt != Format && fmt != CSharpFormat) throw std::runtime_error("not a Glue3D project");

    if (root.contains("variables") && root["variables"].is_object())
    {
        const json& vars = root["variables"];
        for (auto& [fieldName, member] : Stage::ValueFields())
        {
            auto it = vars.find(fieldName);
            if (it != vars.end()) stage.*member = FromJson(*it);
        }
    }
    if (root.contains("lists") && root["lists"].is_object())
    {
        const json& lists = root["lists"];
        for (auto& [fieldName, member] : Stage::ListFields())
        {
            // Glue3D's own built-in texts (the block palette, block colors, import templates) always come from
            // this version, so a project saved by an older version still gets the newest blocks.
            std::string_view f = fieldName;
            if (f == "autocomplete" || f == "textColoration" || f == "preLoaded") continue;
            auto it = lists.find(fieldName);
            if (it == lists.end() || !it->is_array()) continue;
            ScratchList& list = stage.*member;
            list.Items.clear();
            list.Items.reserve(it->size());
            for (const json& item : *it) list.Items.push_back(FromJson(item));
            list.Version++;
        }
    }
    return InfoFrom(root, folder);
}

} // namespace Glue3D::ProjectFiles
