// Host tests for source/util/sphaira_entry.cpp: the file sphaira's GitHub
// menu reads for KitTest, written only when sphaira is on the SD card, the
// same as the file the apps ship in their release zip, rewritten only when
// its keys differ (the user's other keys kept), removed when the setting is
// off.
#include "check.h"
#include "temp_dir.hpp"
#include <cstdlib>
#include <string>

#include <aurore/extern/nlohmann/json.hpp>

#include "kit_app.hpp"
#include "util/paths.hpp"
#include "util/sphaira_entry.hpp"

const char* app::repo_url() { return "https://github.com/JigSawFr/KitTest"; }

using sphaira_entry::Outcome;

static std::string full(const std::string& sd_path)
{
    return paths::sd_root() + sd_path;
}

static void make_dir(const std::string& sd_path)
{
    CHECK(std::system(("mkdir -p '" + full(sd_path) + "'").c_str()) == 0);
}

static nlohmann::json read_entry()
{
    std::string text;
    if (!CHECK(paths::read_file(full(sphaira_entry::sd_path()), text))) return {};
    return nlohmann::json::parse(text, nullptr, false);
}

int main()
{
    TempDir tmp("sphaira_entry", TempDir::Enter);   // paths::sd_root() is ./kittest_data/sd on the host

    CHECK(sphaira_entry::sd_path() == "/config/sphaira/github/kittest.json");

    // The text: the same file PlayGuard ships in its release zip
    // (packaging/sphaira/playguard.json), byte for byte.
    const std::string shipped = R"({
    "name": "PlayGuard",
    "url": "https://github.com/JigSawFr/PlayGuard",
    "assets": [
        {
            "name": "playguard.nro",
            "path": "/switch/playguard/playguard.nro"
        }
    ]
}
)";
    CHECK(sphaira_entry::text("", "PlayGuard", "https://github.com/JigSawFr/PlayGuard", "playguard") == shipped);
    CHECK(sphaira_entry::up_to_date(shipped, "PlayGuard", "https://github.com/JigSawFr/PlayGuard", "playguard"));
    // Another layout, more keys: still up to date.
    CHECK(sphaira_entry::up_to_date(R"({"tag": "latest", "assets": [{"path": "/switch/playguard/playguard.nro", "name": "playguard.nro"}],
                                        "url": "https://github.com/JigSawFr/PlayGuard", "name": "PlayGuard"})",
                                    "PlayGuard", "https://github.com/JigSawFr/PlayGuard", "playguard"));
    CHECK(!sphaira_entry::up_to_date("", "PlayGuard", "u", "playguard"));
    CHECK(!sphaira_entry::up_to_date(R"({"name": "PlayGuard", "url": "u", "assets": []})", "PlayGuard", "u", "playguard"));
    // An existing file: its other keys kept, ours set again.
    {
        const std::string before = R"({"tag": "", "post_install_message": "Hi", "url": "https://example.org/x", "assets": []})";
        const auto j = nlohmann::json::parse(sphaira_entry::text(before, "KitTest", "https://github.com/JigSawFr/KitTest", "kittest"));
        CHECK(j["tag"] == "");
        CHECK(j["post_install_message"] == "Hi");
        CHECK(j["url"] == "https://github.com/JigSawFr/KitTest");
        CHECK(j["assets"][0]["name"] == "kittest.nro");
        CHECK(j["assets"][0]["path"] == "/switch/kittest/kittest.nro");
    }
    // Not JSON, or not an object: replaced by a new one.
    for (const char* bad : { "not json", "[1, 2]" }) {
        const auto j = nlohmann::json::parse(sphaira_entry::text(bad, "KitTest", "u", "kittest"));
        CHECK(j.is_object() && j["name"] == "KitTest" && !j.contains("tag"));
    }

    // No sphaira: nothing written, nothing to remove.
    CHECK(!sphaira_entry::sphaira_found());
    CHECK(sphaira_entry::sync(true) == Outcome::Unchanged);
    CHECK(!paths::file_exists(full(sphaira_entry::sd_path())));
    CHECK(sphaira_entry::sync(false) == Outcome::Unchanged);

    // sphaira's .nro alone is enough; its github folder is made.
    make_dir("/switch/sphaira");
    CHECK(std::system(("touch '" + full("/switch/sphaira/sphaira.nro") + "'").c_str()) == 0);
    CHECK(sphaira_entry::sphaira_found());
    CHECK(sphaira_entry::sync(true) == Outcome::Written);
    CHECK(read_entry()["assets"][0]["name"] == "kittest.nro");
    // Already right: left alone.
    CHECK(sphaira_entry::sync(true) == Outcome::Unchanged);

    // Rewritten by layout alone: no (the release zip's own file stays as it is).
    {
        std::string err;
        CHECK(paths::atomic_write(full(sphaira_entry::sd_path()),
                                  R"({"assets":[{"path":"/switch/kittest/kittest.nro","name":"kittest.nro"}],"url":"https://github.com/JigSawFr/KitTest","name":"KitTest"})", &err));
        CHECK(sphaira_entry::sync(true) == Outcome::Unchanged);
    }
    // A file the user changed: our keys set again, the tag kept.
    {
        std::string err;
        CHECK(paths::atomic_write(full(sphaira_entry::sd_path()), R"({"tag": "", "url": "https://example.org/old"})", &err));
        CHECK(sphaira_entry::sync(true) == Outcome::Written);
        const auto j = read_entry();
        CHECK(j["tag"] == "");
        CHECK(j["url"] == "https://github.com/JigSawFr/KitTest");
        CHECK(sphaira_entry::sync(true) == Outcome::Unchanged);
    }

    // Off: removed, and only ours (sphaira's own files stay).
    CHECK(std::system(("touch '" + full("/config/sphaira/github/repositories.json") + "'").c_str()) == 0);
    CHECK(sphaira_entry::sync(false) == Outcome::Removed);
    CHECK(!paths::file_exists(full(sphaira_entry::sd_path())));
    CHECK(paths::file_exists(full("/config/sphaira/github/repositories.json")));
    CHECK(sphaira_entry::sync(false) == Outcome::Unchanged);

    // sphaira's config folder alone (its .nro elsewhere, as hbmenu.nro).
    CHECK(std::system(("rm -rf '" + full("/switch/sphaira") + "'").c_str()) == 0);
    CHECK(sphaira_entry::sphaira_found());
    CHECK(sphaira_entry::sync(true) == Outcome::Written);

    CHECK(tmp.remove());
    return CHECK_DONE("sphaira entry text, write, keep and remove assertions passed");
}
