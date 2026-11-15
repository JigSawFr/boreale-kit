// sysmodule_flow — puts the sysmodule an app carries in place of the one on
// the SD card while it runs, and puts the previous one back when the new one
// does not answer (the steps are sysmodule::next_step's: shutdown, stop,
// swap, start, check, confirm, or stop the new one, restore, restart). Each
// step runs one after the other; the wait for the new module to answer does
// not block anything (a delay between tries). No screen and no console call
// of its own: what the console does (is it running, launch, terminate, does
// it answer) and what the app does around it (the link's hand-over, the change
// history) are the `Host` the app fills in, so the whole plan is host-tested
// (tests/sysmodule_flow) with a fake console.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "util/sysmodule.hpp"

namespace sysmodule_flow
{

// Whether the module just started answers (the agent: a Hello accepted).
enum class Answer
{
    Waiting,   // not yet: asked again after `check_every_ms`
    Yes,
    Refused,   // it answers, but not for this app (another protocol): `why` says
};

struct Host
{
    std::string sd_root;

    // The console's services for a module (its program id); `launch` and
    // `terminate` give 0 when it went through, else the console's result code.
    std::function<bool(uint64_t tid)>     running;
    std::function<uint32_t(uint64_t tid)> launch;
    std::function<uint32_t(uint64_t tid)> terminate;
    std::function<std::string(uint32_t rc)> rc_text;   // a result code in words

    // Asks a running module to leave what it holds (the agent: the broker,
    // "offline" said), then calls `then` (always, even when it was not asked).
    std::function<void(std::function<void()> then)> leave;
    // Whether the module answers now; fills `why` when Refused.
    std::function<Answer(std::string& why)> answers;
    // `fn` after `ms` milliseconds, off the caller's stack.
    std::function<void(long ms, std::function<void()> fn)> delay;

    // Around the run: the app's own link leaves the module alone from `hold(true)`
    // to the end, then `link_back` hands it over again.
    std::function<void(bool)> hold;
    std::function<void()>     link_back;
    // What went in the change history: "installed", "updated", "rolled_back", "failed".
    std::function<void(const char* what)> record;
    std::function<void(const std::string& line)> log;
    // The module is in place but its metadata could not be written (a warning, not a failure).
    std::function<void(const std::string& why)> warn;

    // What is said when it does not go through, in the app's words.
    std::string busy_text, not_bundled_text, no_answer_text;
    long check_every_ms = 500;
    int  check_tries = 20;   // 10 s
};

struct Outcome
{
    sysmodule::Step end = sysmodule::Step::Failed;   // Done, RolledBack or Failed
    bool            update = false;                  // a module was there before
    std::string     why;                             // when it went wrong
};

// Installs or updates `m` from `bundle` and calls `done` with the outcome. One
// run at a time: another asked meanwhile ends at once with `busy_text`.
void run(const Host& host, const sysmodule::Module& m, const sysmodule::Bundle& bundle,
         std::function<void(const Outcome&)> done);
bool running();

}   // namespace sysmodule_flow
