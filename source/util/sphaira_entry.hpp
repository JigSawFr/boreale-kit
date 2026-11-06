// sphaira_entry — the app's own entry in sphaira's GitHub menu, so sphaira
// offers each release as soon as it is on GitHub, without waiting for a
// store's review. sphaira reads every *.json of /config/sphaira/github/ at
// start-up (its ui/menus/ghdl.cpp): the app keeps one file there, named
// after it, and never touches sphaira's other files.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace sphaira_entry
{

// From the SD card root: "/config/sphaira/github/<APP_ID>.json".
std::string sd_path();

// Whether sphaira is on the SD card: its folder in /config (made at its
// first start, wherever its .nro is) or its .nro where it looks for itself.
bool sphaira_found();

// The file's text. `existing` is the file as it is now ("" when there is
// none): its other keys (a "tag", messages the user added) are kept; the app
// sets "name", "url" and "assets" (the release's <APP_ID>.nro, put in
// /switch/<APP_ID>/), as the file each app ships in its release zip
// (packaging/sphaira/<APP_ID>.json), so the two never undo each other.
std::string text(const std::string& existing, const std::string& name, const std::string& repo_url,
                 const std::string& app_id);
// Whether `existing` already has the app's keys as text() sets them (its
// layout and other keys do not matter).
bool up_to_date(const std::string& existing, const std::string& name, const std::string& repo_url,
                const std::string& app_id);

enum class Outcome
{
    Unchanged,   // nothing to do (already right, or no sphaira and nothing to remove)
    Written,
    Removed,
    Failed,
};

// Brings the file in line with About › Entry in sphaira (`on`): written when
// sphaira is there and the file is missing or differs, removed when off.
// Blocking (SD card): not on the UI thread.
Outcome sync(bool on, std::string* error = nullptr);

}   // namespace sphaira_entry
