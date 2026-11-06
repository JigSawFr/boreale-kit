// ui::app_hooks — what the kit's screens and flows (ui/, view/, activity/,
// action/) ask of the app above them, besides what Aurore's Cadran asks
// (ui::hooks): the app fills this in once at start-up, before any screen
// opens. Each hook left empty does nothing (or answers nothing).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>
#include <functional>
#include <string>
#include <vector>

#include "util/log_upload.hpp"
#include "util/update.hpp"

namespace ui::app_hooks
{

struct Hooks
{
    std::function<aurore::View*()>      make_help_tab;          // Help & diagnostics, for a screen of its own
    std::function<void(const char* what, bool ok)> note_write;   // a write that failed, for the app to say
    std::function<void()>             mode_changed;           // what runs only in developer mode
    std::function<void()>             open_notices;           // the notification bell's list
    std::function<void()>             open_firmware_screen;   // the firmware screen, when the app has one
    std::function<bool()>             lock_screen_up;         // the app's lock screen is open (the title says so)

    // The key under "kit/error/" to say in place of `key` ("read_only"),
    // or "": an app that has another reason for it.
    std::function<std::string(const std::string& key)> error_key;

    // After each check for the app's own update, on the check's thread
    // (blocking is fine): the app may check more with it, the other
    // homebrews' releases for one. Unset: nothing more.
    std::function<void(const update::Result&)> update_checked;

    // Before replacing the running .nro (an update, another build, going
    // back) or, with `hand_over`, before leaving for another store that
    // does it: false stops it, the app having said why (a PIN to enter…).
    // Unset: allowed.
    std::function<bool(bool hand_over)> may_replace_self;

    // Next to the app's own update check (update_flow::check_now) and
    // start-up check: what else the app looks up at the same time (the system
    // update, other homebrews). Unset: nothing more.
    std::function<void()> also_check;
    std::function<void()> also_at_start;

    // Before the patch report is read (once per run): the app prepares what it
    // reads (the simulated SD card's files). Unset: nothing.
    std::function<void()> before_patch_report;

    // The files an app adds to the diagnostic report it sends (name, text):
    // called on the UI thread, it returns what then runs on the worker thread.
    // Unset: none.
    using ReportFiles = std::vector<log_upload::Part>;
    std::function<std::function<ReportFiles()>()> report_files;

    // News for the app's notifications, when it has some: `id` names the
    // entry (set again: replaced), `action` is its button (`button` its
    // label; nullptr: none). Unset: nothing is kept.
    // `action_needed`: the entry asks for something to be done (not only news).
    std::function<void(const std::string& id, const std::string& title, const std::string& text,
                       const std::string& button, std::function<void()> action, bool action_needed)>
                                                   notice_set;
    std::function<void(const std::string& id)> notice_clear;
};

// The one set, filled in at start-up.
Hooks& get();

}   // namespace ui::app_hooks
