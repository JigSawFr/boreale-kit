// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/sysmodule_flow.hpp"

#include <memory>

namespace sysmodule_flow
{

namespace
{
struct Run
{
    Host              host;
    sysmodule::Module module;
    sysmodule::Bundle bundle;
    bool              was_running = false;
    sysmodule::Step   step = sysmodule::Step::Failed;
    int               tries = 0;
    Outcome           out;
    std::function<void(const Outcome&)> done;
};

bool s_running = false;

void log(const Run& r, const std::string& line)
{
    if (r.host.log) r.host.log(line);
}

void record(const Run& r, const char* what)
{
    if (r.host.record) r.host.record(what);
}

void finish(const std::shared_ptr<Run>& r)
{
    s_running = false;
    r->out.end = r->step;
    log(*r, std::string("update: ") + sysmodule::step_name(r->step) + (r->out.why.empty() ? "" : " — " + r->out.why));
    switch (r->step) {
        case sysmodule::Step::Done: record(*r, r->out.update ? "updated" : "installed"); break;
        case sysmodule::Step::RolledBack:
            if (r->out.update) record(*r, "rolled_back");
            break;
        default: record(*r, "failed"); break;
    }
    // The module's session again (or the app's own when it is gone).
    if (r->host.hold) r->host.hold(false);
    if (r->host.link_back) r->host.link_back();
    if (r->done) r->done(r->out);
}

void go(const std::shared_ptr<Run>& r);

void advance(const std::shared_ptr<Run>& r, bool ok)
{
    log(*r, std::string("update: ") + sysmodule::step_name(r->step) + (ok ? " ok" : " failed"));
    r->step = sysmodule::next_step(r->step, ok, r->was_running);
    go(r);
}

std::string rc_words(const Run& r, uint32_t rc)
{
    return r.host.rc_text ? r.host.rc_text(rc) : "result " + std::to_string(rc);
}

// The new module answers, or does not within the tries.
void check(const std::shared_ptr<Run>& r)
{
    std::string why;
    const Answer a = r->host.answers ? r->host.answers(why) : Answer::Waiting;
    if (a == Answer::Yes) {
        advance(r, true);
        return;
    }
    if (a == Answer::Refused) {
        r->out.why = why;
        advance(r, false);
        return;
    }
    if (++r->tries >= r->host.check_tries) {
        r->out.why = r->host.no_answer_text;
        advance(r, false);
        return;
    }
    if (r->host.delay) r->host.delay(r->host.check_every_ms, [r]() { check(r); });
    else check(r);
}

void go(const std::shared_ptr<Run>& r)
{
    using S = sysmodule::Step;
    const sysmodule::Module& m = r->module;
    switch (r->step) {
        case S::Shutdown:
            if (r->host.leave) r->host.leave([r]() { advance(r, true); });
            else advance(r, true);
            return;
        case S::Stop:
        case S::StopNew: {
            const uint32_t rc = r->host.terminate ? r->host.terminate(m.tid) : 0;
            if (r->step == S::Stop && rc != 0) r->out.why = rc_words(*r, rc);
            advance(r, rc == 0);
            return;
        }
        case S::Swap: {
            const auto done = sysmodule::install(r->host.sd_root, m, r->bundle, true);
            if (!done) r->out.why = done.error();
            if (done && !done->warning.empty() && r->host.warn) r->host.warn(done->warning);
            advance(r, done.has_value());
            return;
        }
        case S::Start:
        case S::Restart: {
            const uint32_t rc = r->host.launch ? r->host.launch(m.tid) : 0;
            if (rc != 0 && (r->step == S::Start || r->out.why.empty())) r->out.why = rc_words(*r, rc);
            advance(r, rc == 0);
            return;
        }
        case S::Check:
            r->tries = 0;
            check(r);
            return;
        case S::Restore: {
            // A first install that does not answer is removed; an update
            // gets the previous module back.
            const auto done = r->out.update ? sysmodule::rollback(r->host.sd_root, m)
                                            : sysmodule::uninstall(r->host.sd_root, m);
            if (!done) r->out.why += (r->out.why.empty() ? "" : " — ") + done.error();
            advance(r, done.has_value());
            return;
        }
        case S::Confirm:
            sysmodule::confirm(r->host.sd_root, m);
            advance(r, true);
            return;
        case S::Done:
        case S::RolledBack:
        case S::Failed: finish(r); return;
    }
}
}   // namespace

bool running()
{
    return s_running;
}

void run(const Host& host, const sysmodule::Module& m, const sysmodule::Bundle& bundle,
         std::function<void(const Outcome&)> done)
{
    auto r = std::make_shared<Run>();
    r->host = host;
    r->module = m;
    r->bundle = bundle;
    r->done = std::move(done);
    if (s_running || !bundle.present) {
        r->out.why = s_running ? host.busy_text : host.not_bundled_text;
        if (r->done) r->done(r->out);
        return;
    }
    s_running = true;
    r->out.update = sysmodule::state(host.sd_root, m).installed;
    r->was_running = r->out.update && host.running && host.running(m.tid);
    r->step = sysmodule::first_step(r->was_running);
    log(*r, "update: to " + bundle.version + " (" + bundle.sha256.substr(0, 12) + "), " +
                (r->was_running ? "running" : "not running"));
    // The link leaves the module alone until the end.
    if (host.hold) host.hold(true);
    go(r);
}

}   // namespace sysmodule_flow
