// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/agent_manage_logic.hpp"

namespace agent_manage_logic
{

Summary summarize(const sysmodule::State& s, const sysmodule::Bundle& b)
{
    if (!s.installed) return b.present ? Summary::NotInstalled : Summary::NotBundled;
    return sysmodule::offer(s, b) == sysmodule::Offer::Update ? Summary::Update : Summary::Installed;
}

std::string state_key(Summary s, bool brief)
{
    const char* name = s == Summary::NotBundled ? "not_bundled"
                       : s == Summary::NotInstalled ? "not_installed"
                       : s == Summary::Update       ? "update"
                                                    : "installed";
    if (brief && s == Summary::NotBundled) return "";
    return std::string(brief ? "agent/short/" : "agent/state/") + name;
}

std::string short_sha(const std::string& sha)
{
    return sha.substr(0, 8);
}

}   // namespace agent_manage_logic
