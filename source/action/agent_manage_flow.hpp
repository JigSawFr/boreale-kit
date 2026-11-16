// agent_manage_flow — the shared agent (the sysmodule that holds the console's
// one broker connection, program id sysmodule::AGENT_TID) installed, updated,
// started, stopped and removed by any Boréale app that carries it in its
// romfs (<bundle_dir>/agent/exefs.nsp, version.txt: boreale-kit's
// util/sysmodule). Fills in sysmodule_flow's Host without asking anything of
// the app but its link's hand-over and its change history: the console's
// processes are core/sysmodule_nx.h, the agent's clean stop and its answer
// are bl:link's control commands (Info, Shutdown, Resume: boreale-link's
// bl_relay_nx.h). The screen is activity/agent_activity.hpp.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <string>

#include "action/agent_manage_logic.hpp"
#include "util/sysmodule.hpp"

namespace agent_manage_flow
{

// What the app adds to it; all unset in the simplest app.
struct Hooks
{
    // Where the app carries its sysmodules (the romfs folder holding agent/): "" says none.
    std::function<std::string()> bundle_dir;
    // The app's own link to the agent (a session with it) leaves the agent alone
    // from hold(true) to the end of an install, an update or a stop...
    std::function<void(bool)> hold;
    // ...then comes back (the agent started, updated, or gone for good).
    std::function<void()> link_back;
    // A change worth the app's history: "installed", "updated", "rolled_back",
    // "failed", "started", "stopped", "removed", "boot_on" or "boot_off".
    std::function<void(const char* what)> record;
    // Whether a change may go on now (a PIN to enter…): false stops it, the app
    // having said why. Unset: allowed.
    std::function<bool()> allowed;
};
Hooks& hooks();

// The agent as the screens know it (program id AGENT_TID, resident, "Boreale Link").
const sysmodule::Module& module();

struct Status
{
    sysmodule::State              state;
    sysmodule::Bundle             bundle;
    bool                          running = false;
    agent_manage_logic::Summary   summary = agent_manage_logic::Summary::NotBundled;
};
Status status();

// Each one says how it went (a toast or a dialog) and calls `done` (UI thread)
// when it has ended, whatever the outcome; one at a time (a second asked
// meanwhile is told it is busy).
void install(std::function<void()> done);   // installs, or updates, from the app's copy
void start(std::function<void()> done);
void stop(std::function<void()> done);      // a clean "offline" first
void remove(std::function<void()> done);   // stops it first, then deletes its folder
void set_at_boot(bool on);
bool busy();

}   // namespace agent_manage_flow
