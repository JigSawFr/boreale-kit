// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/agent_manage_flow.hpp"

#include <aurore.hpp>
#include <memory>

#include "action/sysmodule_flow.hpp"
#include "bl_relay.h"
#include "bl_relay_nx.h"
#include "core/sysmodule_nx.h"
#include "kit_text.hpp"
#include "ui/ui.hpp"
#include "util/paths.hpp"

namespace agent_manage_flow
{

namespace
{
constexpr long POLL_MS = 500;   // between two looks at whether the agent has left the broker
constexpr int  POLL_TRIES = 20;

bool s_busy = false;

void record(const char* what)
{
    if (hooks().record) hooks().record(what);
}

void hold(bool on)
{
    if (hooks().hold) hooks().hold(on);
}

void link_back()
{
    if (hooks().link_back) hooks().link_back();
}

bool allowed()
{
    return !hooks().allowed || hooks().allowed();
}

std::string bundle_dir()
{
    return hooks().bundle_dir ? hooks().bundle_dir() : "";
}

// The agent leaves the broker ("offline" said), then `then`: asked through
// bl:link's Shutdown, waited for until it says it is no longer online (an
// agent that does not answer, or is not there, is not waited for).
void leave_poll(int left, std::function<void()> then)
{
    BlRelayInfo info;
    if (left > 0 && bl_relay_info(&info) == 0 && info.online) {
        aurore::delay(POLL_MS, [left, then]() { leave_poll(left - 1, then); });
        return;
    }
    if (then) then();
}

void leave(std::function<void()> then)
{
    if (bl_relay_available() && bl_relay_shutdown() == 0) {
        leave_poll(POLL_TRIES, std::move(then));
        return;
    }
    if (then) then();
}

// The app's way when it has one (hooks().leave), else the kit's.
void leave_agent(std::function<void()> then)
{
    if (hooks().leave) hooks().leave(std::move(then));
    else leave(std::move(then));
}

// Whether the new agent answers: bl:link's Info, with the protocol this app speaks.
sysmodule_flow::Answer answers(std::string& why)
{
    BlRelayInfo info;
    if (bl_relay_info(&info) != 0) return sysmodule_flow::Answer::Waiting;
    if (info.protocol != BL_RELAY_PROTOCOL) {
        why = kit_text::str("agent/other_protocol", (int)info.protocol);
        return sysmodule_flow::Answer::Refused;
    }
    info.version[sizeof(info.version) - 1] = '\0';
    aurore::Logger::info("agent: the agent {} answers", info.version);
    return sysmodule_flow::Answer::Yes;
}

sysmodule_flow::Host host()
{
    sysmodule_flow::Host h;
    h.sd_root = paths::sd_root();
    h.running = [](uint64_t tid) { return sysmodule_running(tid, nullptr); };
    h.launch = [](uint64_t tid) { const Result rc = sysmodule_launch(tid); return R_FAILED(rc) ? (uint32_t)rc : 0u; };
    h.terminate = [](uint64_t tid) { const Result rc = sysmodule_terminate(tid); return R_FAILED(rc) ? (uint32_t)rc : 0u; };
    h.rc_text = [](uint32_t rc) { return ui::rc_text((Result)rc); };
    h.leave = leave_agent;
    h.answers = [](std::string& why) { return hooks().answers ? hooks().answers(why) : answers(why); };
    h.delay = [](long ms, std::function<void()> fn) { aurore::delay(ms, std::move(fn)); };
    h.hold = hold;
    // The agent that was asked to leave comes back (one that was stopped is a
    // new process, back already), then the app's own link.
    h.link_back = []() {
        bl_relay_resume();
        link_back();
    };
    h.record = record;
    h.log = [](const std::string& line) { aurore::Logger::info("agent {}", line); };
    h.warn = [](const std::string& why) {
        aurore::Logger::warning("agent: in place, {}", why);
        aurore::sync([]() { ui::notify(kit_text::str("agent/meta_err"), ui::Toast::Refused); });
    };
    h.busy_text = kit_text::str("agent/busy");
    h.not_bundled_text = kit_text::str("agent/not_bundled");
    h.no_answer_text = kit_text::str("agent/no_answer");
    return h;
}

void tell(const sysmodule_flow::Outcome& o)
{
    switch (o.end) {
        case sysmodule::Step::Done: ui::notify(kit_text::str(o.update ? "agent/updated" : "agent/installed")); break;
        case sysmodule::Step::RolledBack:
            ui::error(kit_text::str(o.update ? "agent/start_failed" : "agent/install_undone", o.why));
            break;
        default: ui::error(kit_text::str("agent/update_failed", o.why)); break;
    }
}

// One operation at a time, with `done` called whatever happens.
bool begin()
{
    if (s_busy || sysmodule_flow::running()) {
        ui::notify(kit_text::str("agent/busy"));
        return false;
    }
    if (!allowed()) return false;
    s_busy = true;
    return true;
}

std::function<void()> finish(std::function<void()> done)
{
    return [done]() {
        s_busy = false;
        if (done) done();
    };
}
}   // namespace

Hooks& hooks()
{
    static Hooks h;
    return h;
}

const sysmodule::Module& module()
{
    static const sysmodule::Module m{ "agent", sysmodule::AGENT_TID, true, "Boreale Link" };
    return m;
}

Status status()
{
    Status s;
    const std::string root = paths::sd_root();
    s.state = sysmodule::state(root, module());
    s.bundle = sysmodule::bundled(bundle_dir(), module());
    s.running = sysmodule_running(module().tid, nullptr);
    s.summary = agent_manage_logic::summarize(s.state, s.bundle);
    return s;
}

bool busy()
{
    return s_busy || sysmodule_flow::running();
}

void install(std::function<void()> done)
{
    if (!begin()) return;
    const auto end = finish(done);
    const Status s = status();
    if (!s.bundle.present) {
        ui::info(kit_text::str("agent/not_bundled"));
        end();
        return;
    }
    ui::notify(kit_text::str(s.state.installed ? "agent/updating" : "agent/installing"));
    sysmodule_flow::run(host(), module(), s.bundle, [end](const sysmodule_flow::Outcome& o) {
        tell(o);
        end();
    });
}

void start(std::function<void()> done)
{
    if (!begin()) return;
    const Result rc = sysmodule_launch(module().tid);
    ui::notify_result(rc, kit_text::str("agent/started"), kit_text::str("agent/error"));
    if (R_SUCCEEDED(rc)) record("started");
    link_back();
    finish(done)();
}

void stop(std::function<void()> done)
{
    if (!begin()) return;
    const auto end = finish(done);
    hold(true);
    leave_agent([end]() {
        const Result rc = sysmodule_terminate(module().tid);
        ui::notify_result(rc, kit_text::str("agent/stopped"), kit_text::str("agent/error"));
        if (R_SUCCEEDED(rc)) record("stopped");
        hold(false);
        link_back();
        end();
    });
}

void remove(std::function<void()> done)
{
    if (!begin()) return;
    const auto end = finish(done);
    const auto erase = [end]() {
        if (const auto removed = sysmodule::uninstall(paths::sd_root(), module()); !removed)
            ui::error(kit_text::str("agent/error") + " — " + removed.error());
        else {
            ui::notify(kit_text::str("agent/removed"));
            record("removed");
        }
        hold(false);
        link_back();
        end();
    };
    if (!sysmodule_running(module().tid, nullptr)) {
        erase();
        return;
    }
    hold(true);
    leave_agent([erase, end]() {
        const Result rc = sysmodule_terminate(module().tid);
        if (R_FAILED(rc)) {
            ui::error(kit_text::str("agent/error"), rc);
            hold(false);
            link_back();
            end();
            return;
        }
        erase();
    });
}

void check_release(std::function<void(const agent_release::Check&)> done)
{
    const Status s = status();
    const std::string sha = s.state.sha256, installed = s.state.version, carried = s.bundle.version;
    ui::in_background("agent release", [sha, installed, carried, done]() {
        const agent_release::Check c = agent_release::check(BL_RELAY_PROTOCOL, sha, installed, carried);
        aurore::sync([c, done]() {
            if (done) done(c);
        });
    });
}

void install_release(const agent_release::Manifest& release, std::function<void()> done)
{
    if (!begin()) return;
    const auto end = finish(done);
    ui::notify(kit_text::str("agent/release/downloading"));
    const std::string dir = paths::data_dir() + "/agent_release";
    ui::in_background("agent release download", [release, dir, end]() {
        const std::string why = agent_release::download(release, dir);
        aurore::sync([why, dir, end]() {
            if (!why.empty()) {
                ui::error(kit_text::str("agent/release/failed", why));
                end();
                return;
            }
            const Status s = status();
            const sysmodule::Bundle bundle = sysmodule::bundled(dir, module());
            if (!bundle.present) {
                ui::error(kit_text::str("agent/release/failed", kit_text::str("agent/not_bundled")));
                end();
                return;
            }
            ui::notify(kit_text::str(s.state.installed ? "agent/updating" : "agent/installing"));
            sysmodule_flow::run(host(), module(), bundle, [end](const sysmodule_flow::Outcome& o) {
                tell(o);
                end();
            });
        });
    });
}

void set_at_boot(bool on)
{
    if (!allowed()) return;
    if (const auto done = sysmodule::set_at_boot(paths::sd_root(), module(), on); !done)
        ui::error(kit_text::str("agent/error") + " — " + done.error());
    else
        record(on ? "boot_on" : "boot_off");
}

}   // namespace agent_manage_flow
