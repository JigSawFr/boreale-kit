// Host tests for source/action/sysmodule_flow.cpp: the update's plan run with
// a fake console on a fake SD card (what is started with which file, what is
// stopped, what the previous module's return looks like), the change history,
// the link's hand-over around it, and what a refused, a damaged, a busy or a
// missing module does.
#include "check.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "action/sysmodule_flow.hpp"
#include "util/paths.hpp"
#include "util/sha256.hpp"

using namespace sysmodule;
using sysmodule_flow::Answer;
using sysmodule_flow::Outcome;

static std::string g_base;
static int g_n = 0;

static const Module AGENT = { "agent", AGENT_TID, true, "Test agent" };

static std::string fresh(const char* what)
{
    std::string root = g_base + "/" + what + std::to_string(g_n++);
    CHECK(paths::ensure_dir(root));
    return root;
}

static std::string sha(const std::string& bytes)
{
    Sha256 h;
    h.update(bytes);
    return h.hex();
}

static Bundle bundle(const std::string& content, const std::string& version, bool damaged = false)
{
    const std::string dir = fresh("romfs");
    CHECK(paths::atomic_write(dir + "/agent/exefs.nsp", content));
    CHECK(paths::atomic_write(dir + "/agent/version.txt",
                              "version=" + version + "\ncommit=abc1234\nsha256=" + sha(damaged ? content + "x" : content) + "\n"));
    return bundled(dir, AGENT);
}

static std::string read(const std::string& path)
{
    std::string text;
    paths::read_file(path, text);
    return text;
}

// The console and the app around the module, as the flow sees them.
struct Fake
{
    std::string sd;
    bool running_now = false;
    unsigned launched = 0, terminated = 0, left = 0;
    uint32_t launch_rc = 0, terminate_rc = 0;
    int waits = 0;                 // Waiting this many times before answering
    bool refuse = false;
    std::vector<std::string> started_with;   // what exefs.nsp held at each launch
    std::vector<std::string> history, warnings, log;
    bool held = false, held_ever = false, link_back = false;
    std::function<void()> on_leave;          // for the busy test

    sysmodule_flow::Host host()
    {
        sysmodule_flow::Host h;
        h.sd_root = sd;
        h.running = [this](uint64_t) { return running_now; };
        h.launch = [this](uint64_t tid) {
            launched++;
            running_now = true;
            started_with.push_back(read(dir(sd, { "agent", tid, true, "" }) + "/exefs.nsp"));
            return launch_rc;
        };
        h.terminate = [this](uint64_t) {
            terminated++;
            running_now = false;
            return terminate_rc;
        };
        h.rc_text = [](uint32_t rc) { return "result " + std::to_string(rc); };
        h.leave = [this](std::function<void()> then) {
            left++;
            if (on_leave) on_leave();
            then();
        };
        h.answers = [this](std::string& why) {
            if (refuse) {
                why = "other protocol 9";
                return Answer::Refused;
            }
            return waits-- > 0 ? Answer::Waiting : Answer::Yes;
        };
        h.delay = [](long, std::function<void()> fn) { fn(); };
        h.hold = [this](bool on) {
            held = on;
            held_ever |= on;
        };
        h.link_back = [this]() { link_back = true; };
        h.record = [this](const char* what) { history.push_back(what); };
        h.log = [this](const std::string& line) { log.push_back(line); };
        h.warn = [this](const std::string& why) { warnings.push_back(why); };
        h.busy_text = "busy";
        h.not_bundled_text = "not bundled";
        h.no_answer_text = "no answer";
        h.check_every_ms = 0;
        h.check_tries = 3;
        return h;
    }
};

// Runs the flow and returns its outcome (the fake host calls everything back at once).
static Outcome run(Fake& f, const Bundle& b)
{
    Outcome out;
    bool called = false;
    sysmodule_flow::run(f.host(), AGENT, b, [&](const Outcome& o) {
        out = o;
        called = true;
    });
    CHECK(called);
    CHECK(!sysmodule_flow::running());
    return out;
}

// An agent already in place, as an earlier install left it.
static void in_place(Fake& f, const Bundle& b, bool running)
{
    CHECK(install(f.sd, AGENT, b, false));
    f.running_now = running;
}

static void test_first_install()
{
    Fake f;
    f.sd = fresh("sd");
    const Bundle v1 = bundle("NSP v1", "1.1.0");
    const Outcome o = run(f, v1);
    CHECK(o.end == Step::Done && !o.update && o.why.empty());
    const State s = state(f.sd, AGENT);
    CHECK(s.installed && s.at_boot && !s.backup && s.sha256 == v1.sha256 && s.version == "1.1.0");
    CHECK(f.launched == 1 && f.terminated == 0 && f.left == 0 && f.started_with == std::vector<std::string>({ "NSP v1" }));
    CHECK(f.history == std::vector<std::string>({ "installed" }));
    CHECK(f.held_ever && !f.held && f.link_back);   // the link left it alone, then took it back
}

static void test_update_while_running()
{
    Fake f;
    f.sd = fresh("sd");
    in_place(f, bundle("NSP v1", "1.1.0"), true);
    const Bundle v2 = bundle("NSP v2", "1.2.0");
    const Outcome o = run(f, v2);
    CHECK(o.end == Step::Done && o.update);
    const State s = state(f.sd, AGENT);
    CHECK(s.sha256 == v2.sha256 && !s.backup && s.version == "1.2.0");
    // Asked to leave, stopped, the new file started: nothing else.
    CHECK(f.left == 1 && f.terminated == 1 && f.launched == 1 && f.started_with == std::vector<std::string>({ "NSP v2" }));
    CHECK(f.history == std::vector<std::string>({ "updated" }) && f.running_now);
}

static void test_slow_to_answer_still_counts()
{
    Fake f;
    f.sd = fresh("sd");
    in_place(f, bundle("NSP v1", "1.1.0"), true);
    f.waits = 2;   // answers on the third try, the last one allowed
    CHECK(run(f, bundle("NSP v2", "1.2.0")).end == Step::Done);
}

static void test_new_one_does_not_answer()
{
    Fake f;
    f.sd = fresh("sd");
    const Bundle v1 = bundle("NSP v1", "1.1.0");
    in_place(f, v1, true);
    f.waits = 1000;
    const Outcome o = run(f, bundle("NSP v2", "1.2.0"));
    // The new one stopped, the previous put back and started again.
    CHECK(o.end == Step::RolledBack && o.update && o.why == "no answer");
    CHECK(f.terminated == 2 && f.launched == 2 && f.started_with == std::vector<std::string>({ "NSP v2", "NSP v1" }));
    const State s = state(f.sd, AGENT);
    CHECK(s.sha256 == v1.sha256 && !s.backup && f.running_now);
    CHECK(f.history == std::vector<std::string>({ "rolled_back" }) && f.link_back && !f.held);
}

static void test_first_install_does_not_answer()
{
    Fake f;
    f.sd = fresh("sd");
    f.waits = 1000;
    const Outcome o = run(f, bundle("NSP v1", "1.1.0"));
    // Nothing was there before: what was put there goes, nothing is started again.
    CHECK(o.end == Step::RolledBack && !o.update && o.why == "no answer");
    CHECK(!state(f.sd, AGENT).installed && f.launched == 1 && !f.running_now);
    CHECK(f.history.empty());
}

static void test_refused_protocol()
{
    Fake f;
    f.sd = fresh("sd");
    const Bundle v1 = bundle("NSP v1", "1.1.0");
    in_place(f, v1, true);
    f.refuse = true;
    const Outcome o = run(f, bundle("NSP v2", "1.2.0"));
    CHECK(o.end == Step::RolledBack && o.why == "other protocol 9" && state(f.sd, AGENT).sha256 == v1.sha256);
}

static void test_damaged_bundle_keeps_the_previous_running()
{
    Fake f;
    f.sd = fresh("sd");
    const Bundle v1 = bundle("NSP v1", "1.1.0");
    in_place(f, v1, true);
    const Outcome o = run(f, bundle("NSP v2", "1.2.0", true));
    // The swap failed with the previous one in place: started again, left alone.
    CHECK(o.end == Step::RolledBack && o.why.find("damaged") != std::string::npos);
    CHECK(state(f.sd, AGENT).sha256 == v1.sha256 && f.running_now && f.launched == 1);
    CHECK(f.started_with == std::vector<std::string>({ "NSP v1" }));
}

static void test_cannot_be_stopped()
{
    Fake f;
    f.sd = fresh("sd");
    const Bundle v1 = bundle("NSP v1", "1.1.0");
    in_place(f, v1, true);
    f.terminate_rc = 7;
    const Outcome o = run(f, bundle("NSP v2", "1.2.0"));
    CHECK(o.end == Step::Failed && o.why == "result 7" && state(f.sd, AGENT).sha256 == v1.sha256);
    CHECK(f.history == std::vector<std::string>({ "failed" }) && f.launched == 0);
}

static void test_busy_and_missing()
{
    Fake f;
    f.sd = fresh("sd");
    in_place(f, bundle("NSP v1", "1.1.0"), true);
    // Another run asked while one is under way ends at once, the first goes on.
    Outcome second;
    bool second_called = false;
    f.on_leave = [&]() {
        CHECK(sysmodule_flow::running());
        sysmodule_flow::run(f.host(), AGENT, bundle("NSP v3", "1.3.0"), [&](const Outcome& o) {
            second = o;
            second_called = true;
        });
    };
    CHECK(run(f, bundle("NSP v2", "1.2.0")).end == Step::Done);
    CHECK(second_called && second.why == "busy" && second.end == Step::Failed);

    // A build that carries no copy.
    Fake g;
    g.sd = fresh("sd");
    const Outcome none = run(g, Bundle{});
    CHECK(none.why == "not bundled" && g.launched == 0 && !g.held_ever && g.history.empty());
}

int main()
{
    char base[] = "/tmp/kit_sysmodule_flow_XXXXXX";
    REQUIRE(mkdtemp(base) != nullptr);
    g_base = base;

    test_first_install();
    test_update_while_running();
    test_slow_to_answer_still_counts();
    test_new_one_does_not_answer();
    test_first_install_does_not_answer();
    test_refused_protocol();
    test_damaged_bundle_keeps_the_previous_running();
    test_cannot_be_stopped();
    test_busy_and_missing();

    const std::string cleanup = std::string("rm -rf '") + base + "'";
    CHECK(std::system(cleanup.c_str()) == 0);
    return CHECK_DONE("sysmodule_flow install, update, rollback, refused, damaged, busy and missing assertions passed");
}
