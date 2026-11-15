// Host tests for source/util/sysmodule.cpp on fake SD cards: what the app
// carries (a bundle with its version.txt, a damaged one), the state on the
// card (installed by hand, at boot, a backup pending), what the screen
// offers, a first install (boot2.flag, toolbox.json, version.txt), an update
// keeping the previous module until confirmed or put back, the boot switch,
// removing a module with everything in its folder, and the update's plan.
#include "check.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "util/sysmodule.hpp"
#include "util/paths.hpp"
#include "util/sha256.hpp"
#include "util/title_id.hpp"

using namespace sysmodule;

// Why a call failed; "" when it did not.
template <class T> static std::string error_of(const std::expected<T, std::string>& r) { return r ? "" : r.error(); }

static std::string g_base;
static int g_n = 0;

// Two modules as an app describes them: one acting once at boot, one staying running.
static const Module RESCUE = { "rescue", 0x4200000000505247ULL, false, "Test rescue" };
static const Module AGENT = { "agent", AGENT_TID, true, "Test agent" };

static std::string fresh(const char* what)
{
    std::string root = g_base + "/" + what + std::to_string(g_n++);
    CHECK(paths::ensure_dir(root));
    return root;
}

static void put(const std::string& path, const std::string& content)
{
    CHECK(paths::atomic_write(path, content));
}

static bool is_dir(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static std::string read(const std::string& path)
{
    std::string text;
    CHECK(paths::read_file(path, text));
    return text;
}

static std::string sha(const std::string& bytes)
{
    Sha256 h;
    h.update(bytes);
    return h.hex();
}

// A bundle as cmake/bundle_sysmodules.cmake writes it.
static std::string bundle(const std::string& content, const std::string& version, bool damaged = false)
{
    const std::string dir = fresh("romfs");
    put(dir + "/rescue/exefs.nsp", content);
    put(dir + "/rescue/version.txt",
        "version=" + version + "\r\ncommit=abc1234\nsha256=" + sha(damaged ? content + "x" : content) + "\n");
    return dir;
}

static void test_table()
{
    CHECK(RESCUE.tid == 0x4200000000505247ULL && !RESCUE.resident && AGENT_TID == 0x4200000000504741ULL);
    CHECK(AGENT.resident && std::string(AGENT.name) == "agent");
    CHECK(title_id::text(0x4200000000505247ULL) == "4200000000505247");
    CHECK(dir("/", RESCUE) == "/atmosphere/contents/4200000000505247");
    CHECK(dir("sd", RESCUE) == "sd/atmosphere/contents/4200000000505247");
    const std::string tb = toolbox_json(RESCUE);
    CHECK(tb.find("\"tid\": \"4200000000505247\"") != std::string::npos);
    CHECK(tb.find("\"requires_reboot\": true") != std::string::npos);
    CHECK(toolbox_json(AGENT).find("\"requires_reboot\": false") != std::string::npos);
}

static void test_bundle_and_state()
{
    const Module& m = RESCUE;
    const Bundle none = bundled(fresh("empty"), m);
    CHECK(!none.present);
    const Bundle b = bundled(bundle("NSP0 v1", "1.1.0"), m);
    CHECK(b.present && b.version == "1.1.0" && b.commit == "abc1234" && b.sha256 == sha("NSP0 v1"));

    const std::string sd = fresh("sd");
    State s = state(sd, m);
    CHECK(!s.installed && !s.at_boot && !s.backup && s.sha256.empty());
    CHECK(offer(s, b) == Offer::Install);
    CHECK(offer(s, none) == Offer::None);

    // Installed by hand from the release zip: no version.txt.
    put(dir(sd, m) + "/exefs.nsp", "NSP0 v0");
    put(dir(sd, m) + "/flags/boot2.flag", "");
    s = state(sd, m);
    CHECK(s.installed && s.at_boot && s.version.empty() && s.sha256 == sha("NSP0 v0"));
    CHECK(offer(s, b) == Offer::Update);
    CHECK(offer(s, none) == Offer::None);
    // A version.txt about another file says nothing.
    put(dir(sd, m) + "/version.txt", "version=9.9.9\nsha256=" + sha("other") + "\n");
    CHECK(state(sd, m).version.empty());
}

static void test_install_update_rollback()
{
    const Module& m = RESCUE;
    const std::string sd = fresh("sd");
    const Bundle v1 = bundled(bundle("NSP0 v1", "1.1.0"), m);

    // First install: in place, at boot, toolbox.json, version.txt.
    CHECK(install(sd, m, v1, false));
    State s = state(sd, m);
    CHECK(s.installed && s.at_boot && !s.backup && s.sha256 == v1.sha256 && s.version == "1.1.0");
    CHECK(read(dir(sd, m) + "/toolbox.json") == toolbox_json(m));
    CHECK(offer(s, v1) == Offer::None);
    CHECK(!paths::file_exists(dir(sd, m) + "/exefs.nsp.new"));

    // The boot choice survives an update.
    CHECK(set_at_boot(sd, m, false));
    CHECK(!state(sd, m).at_boot);
    CHECK(set_at_boot(sd, m, false));   // already off
    const Bundle v2 = bundled(bundle("NSP0 v2", "1.2.0"), m);
    CHECK(offer(state(sd, m), v2) == Offer::Update);
    CHECK(install(sd, m, v2, true));
    s = state(sd, m);
    CHECK(s.sha256 == v2.sha256 && s.version == "1.2.0" && s.backup && !s.at_boot);
    CHECK(read(dir(sd, m) + "/exefs.nsp.bak") == "NSP0 v1");

    // The new one did not start: the previous one goes back.
    CHECK(rollback(sd, m));
    s = state(sd, m);
    CHECK(s.sha256 == v1.sha256 && !s.backup && s.version.empty());
    CHECK(!error_of(rollback(sd, m)).empty());   // nothing left to put back

    // Updated again, and confirmed: the backup goes.
    CHECK(install(sd, m, v2, true));
    confirm(sd, m);
    s = state(sd, m);
    CHECK(s.sha256 == v2.sha256 && !s.backup);

    // version.txt cannot be written (a folder with a file in its place): the
    // module is in place all the same, and the failure said.
    const std::string version = dir(sd, m) + "/version.txt";
    std::remove(version.c_str());
    CHECK(paths::ensure_dir(version + "/in_the_way"));
    const auto kept = install(sd, m, v1, false);
    CHECK(kept && kept->warning.contains("version.txt"));
    s = state(sd, m);
    CHECK(s.sha256 == v1.sha256 && s.version.empty());
    CHECK(std::system(("rm -rf '" + version + "'").c_str()) == 0);
    const auto back = install(sd, m, v2, false);
    CHECK(back && back->warning.empty());   // v2 back, as below expects

    // A damaged bundle installs nothing and keeps what is there.
    const Bundle bad = bundled(bundle("NSP0 v3", "1.3.0", true), m);
    CHECK(bad.present);
    CHECK(error_of(install(sd, m, bad, false)).contains("damaged"));
    CHECK(state(sd, m).sha256 == v2.sha256);
    const Bundle absent;
    CHECK(!install(sd, m, absent, false));

    // Removed with everything in its folder.
    put(dir(sd, m) + "/LICENSE.txt", "GPL");
    CHECK(set_at_boot(sd, m, true));
    CHECK(uninstall(sd, m));
    CHECK(!is_dir(dir(sd, m)) && !state(sd, m).installed);
    CHECK(uninstall(sd, m));   // already gone
}

// Runs the update plan with the steps that fail, and returns the steps taken.
static std::vector<Step> plan(bool running, std::vector<Step> failing)
{
    std::vector<Step> out;
    Step s = first_step(running);
    for (int guard = 0; guard < 20; guard++) {
        out.push_back(s);
        if (step_final(s)) return out;
        bool ok = true;
        for (Step f : failing) ok &= f != s;
        s = next_step(s, ok, running);
    }
    CHECK(!"the update plan does not end");
    return out;
}

static void test_update_plan()
{
    using S = Step;
    // Running: stopped cleanly, swapped, started, answering, confirmed.
    CHECK(plan(true, {}) == std::vector<S>({ S::Shutdown, S::Stop, S::Swap, S::Start, S::Check, S::Confirm, S::Done }));
    // Not running: no stop; started and checked all the same.
    CHECK(plan(false, {}) == std::vector<S>({ S::Swap, S::Start, S::Check, S::Confirm, S::Done }));
    // A clean stop refused: stopped anyway.
    CHECK(plan(true, { S::Shutdown }).back() == S::Done);
    // Cannot be stopped: nothing changed.
    CHECK(plan(true, { S::Stop }) == std::vector<S>({ S::Shutdown, S::Stop, S::Failed }));
    // The swap failed (the previous one still in place): started again.
    CHECK(plan(true, { S::Swap }) == std::vector<S>({ S::Shutdown, S::Stop, S::Swap, S::Restart, S::RolledBack }));
    CHECK(plan(false, { S::Swap }) == std::vector<S>({ S::Swap, S::Failed }));
    // The new one does not answer: stopped, the previous one back and started.
    CHECK(plan(true, { S::Check }) == std::vector<S>({ S::Shutdown, S::Stop, S::Swap, S::Start, S::Check, S::StopNew,
                                                        S::Restore, S::Restart, S::RolledBack }));
    CHECK(plan(true, { S::Start }) == std::vector<S>({ S::Shutdown, S::Stop, S::Swap, S::Start, S::StopNew, S::Restore,
                                                        S::Restart, S::RolledBack }));
    // Not running before: put back, left stopped.
    CHECK(plan(false, { S::Check }) == std::vector<S>({ S::Swap, S::Start, S::Check, S::StopNew, S::Restore, S::RolledBack }));
    // Nothing to put back, or the previous one does not start again.
    CHECK(plan(true, { S::Check, S::Restore }).back() == S::Failed);
    CHECK(plan(true, { S::Check, S::Restart }).back() == S::Failed);
    // StopNew going wrong changes nothing: the files are put back anyway.
    CHECK(plan(true, { S::Check, S::StopNew }).back() == S::RolledBack);
    CHECK(std::string(step_name(S::RolledBack)) == "rolled_back" && step_final(S::Failed) && !step_final(S::Check));
}

int main()
{
    char base[] = "/tmp/kit_sysmodule_XXXXXX";
    REQUIRE(mkdtemp(base) != nullptr);
    g_base = base;

    test_table();
    test_bundle_and_state();
    test_install_update_rollback();
    test_update_plan();

    const std::string cleanup = std::string("rm -rf '") + base + "'";
    CHECK(std::system(cleanup.c_str()) == 0);
    return CHECK_DONE("sysmodule table, bundle, state, install, update, rollback, removal and update plan assertions passed");
}
