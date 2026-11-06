// kit_settings — the settings the kit reads and writes, the same in every
// app: an app's Config derives from Settings (so its own config.json code
// keeps its names) and gives the kit the one instance and the way to save it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore/cadran/ui/prefs.hpp>
#include <functional>
#include <string>

namespace kit_settings
{

struct Settings
{
    // The look Aurore's Cadran draws (ui::use_prefs): the language, the theme
    // and its palette, the text size, the effects and the screens' glow.
    ui::Prefs   look;
    std::string crypto = "auto";       // Developer › Encryption: auto, software or hardware (util/crypto_backend)
    bool        frame_stats = false;   // Developer › frame statistics (action/frame_stats_flow)
    // How an update is installed: "" (not chosen yet: asked on the first
    // update), "inplace" (the app downloads it and replaces itself), "auto",
    // "sphaira", "appstore", "manual".
    std::string update_via;
    bool        update_daily = false;  // check for updates at start-up, once a day
    std::string update_checked;        // "YYYY-MM-DD" of the last check, any kind
    // The latest release the last check found ("1.2.0"): a newer one than the
    // running version is offered at each start-up (action/update_flow) until
    // installed or postponed. update_snooze_version / _until: "Later" on that
    // screen, for that version until that day ("YYYY-MM-DD", 30 days at most).
    std::string update_latest;
    std::string update_snooze_version;
    std::string update_snooze_until;
    // The saved reports and the crash files kept, each folder on its own
    // (util/log_files.hpp; 0: no limit): the newest logs_keep, those of the
    // last logs_keep_days days, the newest that fit in logs_keep_mb MB.
    int         logs_keep       = 20;
    int         logs_keep_days  = 0;
    int         logs_keep_mb    = 0;
    std::string dev_builds_order = "newest";   // Install another build's pull requests: "newest" or "number"
    // The monthly "Support the app" reminder (off stays off, updates
    // included) and "What's new" once after each update (util/support.hpp).
    // support_reminded: "YYYY-MM-DD" of the last reminder (or of when the
    // month started); seen_version: the version whose "What's new" was handled.
    bool        support_reminder = true;
    std::string support_reminded;
    std::string seen_version;
};

// The app's instance (its config::get()).
Settings& get();

// What ui::save_config writes: the app's settings as they are now, taken on
// the calling (UI) thread, returned as the call that writes them (on the
// write queue's thread; true when written). Empty when they cannot be saved
// at all (the app's previous file could not be moved aside).
std::function<bool()> snapshot();

}   // namespace kit_settings
