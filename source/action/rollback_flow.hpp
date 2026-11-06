// rollback_flow — going back to the previous build (util/rollback.hpp).
// At start-up, before Aurore (so a build that crashes in its UI still
// gets there): when the last start of a build just installed never reached
// its first screen, a text screen offers the previous build; A asks for
// the PIN (the system's screen, when one is set), swaps the two .nro files
// and starts it again through hbloader, B goes on. At any other time,
// Install another build has a line that does the same, after the PIN.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <string>

namespace rollback_flow
{

// What an app adds to it: the data's layout (its migrations) and a gate. All
// unset in the simplest app: going back then only swaps the files.
struct Hooks
{
    // The migration level a release of the app knows ("1.3.0"), -1 when not
    // known: for builds installed before previous_build.json kept it.
    std::function<int(const std::string& label)> release_level;
    // Steps that undo (or a cache that is read again) between the data's
    // level and the previous build's: no loss to warn about.
    std::function<bool(int level, int previous_level)> undoable;
    // In place of migrations::earlier_build_next, when the app runs the
    // previous build's migrations again its own way.
    std::function<void(int previous_level, const std::function<void(bool warning, const std::string& line)>& log)> going_back;
    // At the start-up offer, before the swap: "" to go on, else why not (the
    // PIN was not entered…), which is said on the text screen.
    std::function<std::string(const std::string& previous)> boot_gate;
    // A start that went well after one that crashed following a version
    // change: the app checks its migrations from the old level to the new.
    std::function<void()> verify_all;
};
Hooks& hooks();

// The start-up check, first thing in main(). True when the app must end
// now (the previous build is in place, hbloader starts it).
bool at_start();

// The first screen is up: the start of the build installed went well (the
// notification bell says it was installed).
void started();

// The main screen: when the start-up went back to the previous build (or
// Install another build did), the notification bell says so, once.
void note_went_back();

// The running build's name: "1.3.0 (#318, c3fadd3)" (a pull request's),
// "1.3.0 (8b1e468)", "1.3.0" (a release's).
std::string running_label();

// A build was installed in place of the running one: the running one's
// name is kept as the previous build's, and the next start is checked.
void installed(const std::string& installed_label);

// The previous build's name, when there is one to go back to.
// `newer_layout`, when given: the data is in a newer layout than that build
// knows (its settings may not be found): said before going back.
bool previous(std::string* label, bool* newer_layout = nullptr);

// Goes back to it now: swaps the files, then restarts on it through
// hbloader (or says to restart by hand). On the UI thread; the caller asked
// for the confirmation and the PIN.
void go_back_now();

}   // namespace rollback_flow
