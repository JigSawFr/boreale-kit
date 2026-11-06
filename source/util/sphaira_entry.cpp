// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/sphaira_entry.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#include <aurore/extern/nlohmann/json.hpp>

#include "app_id.h"
#include "kit_app.hpp"
#include "util/paths.hpp"

namespace sphaira_entry
{

namespace
{
constexpr const char* DIR = "/config/sphaira/github";
// Where sphaira looks for its own .nro (as util/launcher.cpp).
const char* const SPHAIRA_NRO[] = { "/switch/sphaira/sphaira.nro", "/switch/sphaira.nro" };

std::string on_sd(const std::string& sd_path)
{
    std::string root = paths::sd_root();
    if (!root.empty() && root.back() == '/') root.pop_back();
    return root + sd_path;
}

bool is_dir(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
}   // namespace

std::string sd_path()
{
    return std::string(DIR) + "/" APP_ID ".json";
}

bool sphaira_found()
{
    if (is_dir(on_sd("/config/sphaira"))) return true;
    for (const char* p : SPHAIRA_NRO)
        if (paths::file_exists(on_sd(p))) return true;
    return false;
}

namespace
{
// The keys the app sets, as in packaging/sphaira/<app>.json of each app (the
// same file, shipped in the release zip).
nlohmann::ordered_json ours(const std::string& name, const std::string& repo_url, const std::string& app_id)
{
    nlohmann::ordered_json j = nlohmann::ordered_json::object();
    j["name"]   = name;
    j["url"]    = repo_url;
    j["assets"] = nlohmann::ordered_json::array(
        { { { "name", app_id + ".nro" }, { "path", "/switch/" + app_id + "/" + app_id + ".nro" } } });
    return j;
}

nlohmann::ordered_json parsed(const std::string& text)
{
    auto j = nlohmann::ordered_json::parse(text, nullptr, false);
    return j.is_object() ? j : nlohmann::ordered_json::object();
}
}   // namespace

std::string text(const std::string& existing, const std::string& name, const std::string& repo_url,
                 const std::string& app_id)
{
    nlohmann::ordered_json j = parsed(existing);
    const nlohmann::ordered_json set = ours(name, repo_url, app_id);
    for (const auto& [key, value] : set.items()) j[key] = value;
    return j.dump(4) + "\n";
}

bool up_to_date(const std::string& existing, const std::string& name, const std::string& repo_url,
                const std::string& app_id)
{
    const nlohmann::ordered_json j = parsed(existing);
    const nlohmann::ordered_json set = ours(name, repo_url, app_id);
    for (const auto& [key, value] : set.items())
        // Compared as plain JSON: the order of an asset's keys does not matter.
        if (!j.contains(key) || nlohmann::json::parse(j.at(key).dump()) != nlohmann::json::parse(value.dump()))
            return false;
    return true;
}

Outcome sync(bool on, std::string* error)
{
    const std::string path = on_sd(sd_path());
    if (!on) {
        if (!paths::file_exists(path)) return Outcome::Unchanged;
        if (std::remove(path.c_str()) == 0) return Outcome::Removed;
        if (error) *error = std::strerror(errno);
        return Outcome::Failed;
    }
    if (!sphaira_found()) return Outcome::Unchanged;
    std::string existing;
    const bool had = paths::file_exists(path) && paths::read_file(path, existing);
    if (had && up_to_date(existing, APP_NAME, app::repo_url(), APP_ID)) return Outcome::Unchanged;
    const std::string wanted = text(had ? existing : "", APP_NAME, app::repo_url(), APP_ID);
    if (!paths::ensure_dir(on_sd(DIR))) {
        if (error) *error = std::strerror(errno);
        return Outcome::Failed;
    }
    return paths::atomic_write(path, wanted, error) ? Outcome::Written : Outcome::Failed;
}

}   // namespace sphaira_entry
