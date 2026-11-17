// sysmodule — an app's optional sysmodules on the SD card, installed, updated,
// put back and removed by the app that carries them (PlayGuard's recovery
// module and the shared agent; any Boréale app that carries the agent). Each one
// lives in atmosphere/contents/<program id>/ (exefs.nsp, flags/boot2.flag to
// start it at boot, toolbox.json for the sysmodule managers, version.txt); the
// app carries a copy of each in its romfs and installs, updates (keeping the
// previous one until the new one is known to start), puts back or removes
// them. No UI and no console calls: host-tested (tests/sysmodule) on a fake SD
// card; starting and stopping a module is the app's (its console service).
// The app describes its modules (`Module`); the kit knows the shared agent's
// program id (AGENT_TID).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace sysmodule
{

// The shared agent's program id (in the range homebrew sysmodules use).
inline constexpr uint64_t AGENT_TID = 0x4200000000504741ULL;

struct Module
{
    const char* name;      // "agent": the romfs folder and the app's i18n keys
    uint64_t    tid;       // program id
    bool        resident;  // stays running (can be started and stopped now)
    const char* title;     // toolbox.json's name
};

std::string dir(const std::string& sd_root, const Module& m);     // <root>/atmosphere/contents/<tid>

// The copy the app carries: <bundle_dir>/<name>/exefs.nsp and version.txt
// ("version=", "commit=", "sha256=" lines).
struct Bundle
{
    bool        present = false;
    std::string nsp;       // its path
    std::string version, commit, sha256;
};
Bundle bundled(const std::string& bundle_dir, const Module& m);

// What is on the SD card.
struct State
{
    bool        installed = false;   // exefs.nsp
    bool        at_boot   = false;   // flags/boot2.flag
    bool        backup    = false;   // exefs.nsp.bak: an update not confirmed yet
    std::string sha256;              // of exefs.nsp, "" when not installed
    std::string version;             // from version.txt ("" when installed by hand)
};
State state(const std::string& sd_root, const Module& m);

// What the screen offers.
enum class Offer
{
    None,      // nothing to do (up to date, or not in this build and not installed)
    Install,   // not installed, the app carries it
    Update,    // installed, another build than the one the app carries
};
Offer offer(const State& s, const Bundle& b);

// Applied, or why not.
using Applied = std::expected<void, std::string>;

// An install that went through.
struct Installed
{
    // Set when the module is in place but toolbox.json or version.txt could
    // not be written (its version then unknown: the install is offered again).
    std::string warning;
};

// Copies the bundled module in: exefs.nsp.new (checked against the bundle's
// SHA-256), the installed one moved to exefs.nsp.bak, the new one in place;
// toolbox.json and version.txt written; boot2.flag created on a first
// install (an update keeps the choice). With `keep_backup` the .bak stays
// until confirm() or rollback() (the agent: until it is seen running).
[[nodiscard]] std::expected<Installed, std::string> install(const std::string& sd_root, const Module& m, const Bundle& b,
                                                            bool keep_backup);
// The new module works: the backup goes.
void confirm(const std::string& sd_root, const Module& m);
// Puts exefs.nsp.bak back in place. An error when there is none.
[[nodiscard]] Applied rollback(const std::string& sd_root, const Module& m);

[[nodiscard]] Applied set_at_boot(const std::string& sd_root, const Module& m, bool on);
// Removes the module's whole folder.
[[nodiscard]] Applied uninstall(const std::string& sd_root, const Module& m);

std::string toolbox_json(const Module& m);

// Updating a module that stays running (the agent), as steps the caller
// carries out one by one, saying each time whether it went through: asked to
// stop cleanly, stopped, swapped (the previous one kept), started, seen
// answering (the agent: a Hello accepted within 10 s), then the backup goes.
// When the new one does not start or answer, it is stopped, the previous one
// put back and started again.
enum class Step
{
    Shutdown,   // ask it to leave the broker (bl:link's Shutdown)
    Stop,
    Swap,       // install(keep_backup)
    Start,
    Check,      // it answers
    StopNew,    // the new one did not answer: stopped (whether it runs or not)
    Restore,    // rollback()
    Restart,    // the previous one started again
    Confirm,    // confirm()
    Done,       // updated
    RolledBack, // not updated: the previous one is back (and runs again when it ran)
    Failed,     // not updated, and the previous one could not be put back or started
};
Step first_step(bool running);
Step next_step(Step done, bool ok, bool was_running);
const char* step_name(Step s);
bool step_final(Step s);

}   // namespace sysmodule
