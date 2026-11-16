// agent_manage_logic — what the agent screen says of the shared agent's state
// (installed or not, the build the app carries, whether they differ), no UI
// and no console call: host-tested (tests/agent_manage_logic).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

#include "util/sysmodule.hpp"

namespace agent_manage_logic
{

enum class Summary
{
    NotBundled,    // not on the card, and this build of the app does not carry it
    NotInstalled,  // not on the card, the app carries it
    Update,        // on the card, the app carries another build
    Installed,     // on the card, the same build (or one this app does not carry)
};

Summary summarize(const sysmodule::State& s, const sysmodule::Bundle& b);

// The catalog's key for it: "agent/state/<name>" (a line of its own) or
// "agent/short/<name>" (one word, for a cell that opens the screen); "" for
// a summary with no short form (not carried: nothing to say there).
std::string state_key(Summary s, bool brief);

// The first 8 characters of a SHA-256 ("" stays empty).
std::string short_sha(const std::string& sha);

}   // namespace agent_manage_logic
