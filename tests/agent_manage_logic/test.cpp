// Host tests for action/agent_manage_logic: the four states the agent screen
// tells apart and the catalog keys that say them.
#include "check.h"

#include "action/agent_manage_logic.hpp"

using agent_manage_logic::Summary;

static sysmodule::Bundle bundle(bool present, const char* sha)
{
    sysmodule::Bundle b;
    b.present = present;
    b.sha256 = sha;
    b.version = "1.0.0";
    return b;
}

static sysmodule::State state(bool installed, const char* sha)
{
    sysmodule::State s;
    s.installed = installed;
    s.sha256 = sha;
    s.version = installed ? "1.0.0" : "";
    return s;
}

int main()
{
    // Not on the card: offered when the app carries it, else nothing to offer.
    CHECK(agent_manage_logic::summarize(state(false, ""), bundle(true, "aa")) == Summary::NotInstalled);
    CHECK(agent_manage_logic::summarize(state(false, ""), bundle(false, "")) == Summary::NotBundled);
    // On the card: the same build, another build, or a copy this app does not carry.
    CHECK(agent_manage_logic::summarize(state(true, "aa"), bundle(true, "aa")) == Summary::Installed);
    CHECK(agent_manage_logic::summarize(state(true, "aa"), bundle(true, "bb")) == Summary::Update);
    CHECK(agent_manage_logic::summarize(state(true, "aa"), bundle(false, "")) == Summary::Installed);

    CHECK(agent_manage_logic::state_key(Summary::Update, false) == "agent/state/update");
    CHECK(agent_manage_logic::state_key(Summary::Installed, true) == "agent/short/installed");
    CHECK(agent_manage_logic::state_key(Summary::NotBundled, false) == "agent/state/not_bundled");
    CHECK(agent_manage_logic::state_key(Summary::NotBundled, true).empty());

    CHECK(agent_manage_logic::short_sha("0123456789abcdef") == "01234567");
    CHECK(agent_manage_logic::short_sha("").empty());

    CHECK_DONE("agent_manage_logic: four states, their keys, the short hash");
}
