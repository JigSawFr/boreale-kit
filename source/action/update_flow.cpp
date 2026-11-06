// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/update_flow.hpp"

#include <iterator>
#include <string_view>

#include "action/dev_build_flow.hpp"
#include "activity/update_activity.hpp"
#include "kit_app.hpp"
#include "kit_text.hpp"
#include "ui/app_hooks.hpp"
#include "ui/ui.hpp"
#include "kit_settings.hpp"
#include "util/date.hpp"
#include "util/launcher.hpp"

using namespace aurore::literals;

namespace update_flow
{

namespace
{
// The values of About › Update with, in the order listed ("" is none chosen).
constexpr std::string_view METHODS[] = { "inplace", "auto", "sphaira", "appstore", "manual" };

// The start-up screen waits for what is in front of the main screen (first
// steps, What's new, a question) to close: tried every 2 s, for a minute.
constexpr long WAIT_STEP_MS = 2000;
constexpr int  WAIT_STEPS   = 30;

bool  s_checking = false;   // the release's details or the store's version asked
bool  s_check_running = false;   // the update check: one at a time, whoever started it
bool  s_checked = false;    // s_offer is this session's, from the server
Offer s_offer;              // the last check's, for update_now()
// Those waiting for the check under way: each told its result.
std::vector<std::function<void(const Offer&)>> s_waiting;

bool busy()
{
    return s_checking || s_check_running;
}

std::string store_name(launcher::Store store)
{
    switch (store) {
        case launcher::Store::Sphaira:  return "sphaira";
        case launcher::Store::AppStore: return kit_text::str("update/appstore");
        default:                        return "";
    }
}

bool newer(const update::Result& r)
{
    return r.verdict == update::Verdict::UpdateSupports || r.verdict == update::Verdict::UpdateNoSupport;
}

update::Snooze snooze()
{
    const auto& cfg = kit_settings::get();
    return update::Snooze{ cfg.update_snooze_version, cfg.update_snooze_until };
}

void show_result(const Offer& o)
{
    switch (o.result.verdict) {
        case update::Verdict::UpdateSupports:
        case update::Verdict::UpdateNoSupport:
            UpdateActivity::show(o);
            break;
        case update::Verdict::UpToDate:
            ui::info(kit_text::str("update/up_to_date", app::version()));
            break;
        case update::Verdict::NoRelease:
            ui::info(kit_text::str("update/no_release", app::version()));
            break;
        default:
            ui::error(kit_text::str("update/failed", o.result.error));
            break;
    }
}

// The notifications' entry for a newer version `latest` ("" or not
// newer: none). Information: Later on the update screen does not hide it.
void note_latest(const std::string& latest)
{
    const auto& h = ui::app_hooks::get();
    if (latest.empty() || update::compare_versions(latest, app::version()) <= 0) {
        if (h.notice_clear) h.notice_clear("update");
        return;
    }
    if (h.notice_set)
        h.notice_set("update", kit_text::str("update/headline", latest),
                     kit_text::str("update/running", app::version()), kit_text::str("update/update"),
                     []() { update_now(); }, false);
}

// Remembers the day of a check that reached the server, and what it found.
void record_check(const update::Result& r)
{
    if (r.verdict == update::Verdict::Unknown) return;
    note_latest(r.verdict == update::Verdict::NoRelease ? "" : r.latest.version);
    auto& cfg = kit_settings::get();
    cfg.update_checked = ui::today_date();
    cfg.update_latest  = r.verdict == update::Verdict::NoRelease ? "" : r.latest.version;
    ui::save_config();
}

// Asks the server (or joins the request under way), then tells `done`.
void run_check(std::function<void(const Offer&)> done)
{
    s_waiting.push_back(std::move(done));
    if (s_check_running) return;
    s_check_running = true;
    const uint32_t fw = sysinfo_hos_version();
    ui::in_background("update check", [fw]() {
        Offer o;
        o.result = update::check(fw);
        if (const auto& checked = ui::app_hooks::get().update_checked) checked(o.result);
        // The newer release's file and notes, for the update screen and an
        // update in place (shown without its notes when they cannot be read).
        if (newer(o.result)) {
            const auto release = dev_builds::fetch_release();
            if (release) o.release = *release;
            o.has_release = release && update::compare_versions(o.release.version, o.result.latest.version) == 0;
            if (!release) aurore::Logger::warning("update: no release details ({})", release.error());
        }
        aurore::sync([o]() {
            s_check_running = false;
            s_offer = o;
            s_checked = o.result.verdict != update::Verdict::Unknown;
            record_check(o.result);
            std::vector<std::function<void(const Offer&)>> waiting;
            waiting.swap(s_waiting);
            for (const auto& tell : waiting) tell(o);
        });
    });
}

// Shows the start-up screen once `main` is on top, waiting for what is in
// front of it to close.
void show_when_free(aurore::Activity* main, const Offer& o, int steps_left)
{
    if (ui::on_top(main)) {
        // Not after another screen of its own this start (First steps): the next.
        if (ui::claim_start_screen("the update screen")) UpdateActivity::show(o);
        return;
    }
    if (steps_left <= 0) return;   // still busy: the next start-up
    aurore::delay(WAIT_STEP_MS, [main, o, steps_left]() { show_when_free(main, o, steps_left - 1); });
}

void install_in_place(const dev_builds::Build& b)
{
    ui::confirm(kit_text::str("update/inplace_body", b.version), kit_text::str("update/inplace_confirm"),
                [b]() { dev_build_flow::install_now(b); });
}

void update_in_place()
{
    if (s_offer.has_release && newer(s_offer.result)) {
        install_in_place(s_offer.release);
        return;
    }
    // The details could not be read during the check: fetched now.
    if (busy()) return;
    s_checking = true;
    ui::notify(kit_text::str("update/checking"));
    ui::in_background("update release", []() {
        auto release = dev_builds::fetch_release();
        aurore::sync([release]() {
            s_checking = false;
            if (!release) ui::error(kit_text::str("update/failed", release.error()));
            else if (update::compare_versions(release->version, app::version()) <= 0)
                ui::info(kit_text::str("update/up_to_date", app::version()));
            else install_in_place(*release);
        });
    });
}

void hand_over(const launcher::Target& target, const std::string& manual)
{
    const std::string name = store_name(target.store);
    ui::confirm(kit_text::str("update/open_body", name), kit_text::str("update/open_confirm", name),
                [target, manual]() {
                    const auto& may = ui::app_hooks::get().may_replace_self;
                    if (may && !may(true)) return;
                    if (launcher::launch(target)) aurore::Application::quit();
                    else ui::info(manual);
                });
}

// Hands over to sphaira or the App Store; explains how to update by hand
// when neither can be opened. When the App Store installed the app, its
// catalogue is asked first: it publishes a release after a review, days
// after GitHub, and a store that still has the version installed would show
// nothing to update. Then the app offers to update itself instead.
void open_store()
{
    const launcher::Target target = launcher::find(kit_settings::get().update_via);
    const std::string manual = kit_text::str("update/manual_body", std::string(app::repo_url()) + "/releases");
    if (target.store == launcher::Store::None || !launcher::can_launch()) {
        ui::info(manual);
        return;
    }
    const std::string latest = kit_settings::get().update_latest;
    if (latest.empty() || !launcher::installed_by_store() || busy()) {
        hand_over(target, manual);
        return;
    }
    s_checking = true;
    ui::in_background("store version", [target, manual, latest]() {
        std::string err;
        const std::string listed = launcher::store_version(&err);
        aurore::sync([target, manual, latest, listed, err]() {
            s_checking = false;
            // Not known (offline, not listed yet): the store, as asked.
            if (listed.empty() || update::compare_versions(listed, latest) >= 0) {
                if (listed.empty()) aurore::Logger::warning("update: the store's version unknown ({})", err);
                hand_over(target, manual);
                return;
            }
            ui::confirm(kit_text::str("update/store_behind", store_name(launcher::Store::AppStore), listed, latest),
                        kit_text::str("update/store_behind_confirm"), []() { update_in_place(); });
        });
    });
}
}   // namespace

void check_now()
{
    // Pressing again while it runs says so, instead of starting another
    // request and stacking a second result dialog.
    ui::notify(kit_text::str("update/checking"));
    if (busy()) return;
    run_check([](const Offer& o) { show_result(o); });
    if (const auto& also = ui::app_hooks::get().also_check) also();
}

void check_once(std::function<void(const Offer&)> done)
{
    if (s_checked && !s_check_running) {
        // Next frame, as after a request.
        aurore::sync([done]() { done(s_offer); });
        return;
    }
    run_check(std::move(done));
}

void at_start(aurore::Activity* main)
{
    const auto& cfg = kit_settings::get();
    const std::string today = ui::today_date();
    const bool pending = update::offer_at_start(cfg.update_latest, app::version(), snooze(), today);
    const bool due = cfg.update_daily && cfg.update_checked != today;
    note_latest(cfg.update_latest);   // what the last check found, until the next one
    if (const auto& also = ui::app_hooks::get().also_at_start) also();
    if (s_checking || !(pending || due)) return;
    // Quietly: only a newer version, not postponed, is worth a word.
    check_once([main, today](const Offer& o) {
        if (newer(o.result) && update::offer_at_start(o.result.latest.version, app::version(), snooze(), today))
            show_when_free(main, o, WAIT_STEPS);
    });
}

void update_now()
{
    const std::string& via = kit_settings::get().update_via;
    if (via.empty()) choose_method([]() { update_now(); }, true);
    else if (via == "inplace") update_in_place();
    else open_store();
}

void choose_method(std::function<void()> then, bool first)
{
    // Installed by the App Store: updated where it came from, so the store's
    // record follows (sphaira, else the App Store: both read its catalogue).
    const std::string_view recommended = launcher::installed_by_store() ? "auto" : METHODS[0];
    std::vector<std::string> labels;
    for (const std::string_view m : METHODS) {
        std::string label = kit_text::str("tools/update_via_values/" + std::string(m));
        if (first && m == recommended) label = kit_text::str("update/recommended", label);
        labels.push_back(label);
    }
    int selected = 0;
    for (size_t i = 0; i < std::size(METHODS); i++)
        if (METHODS[i] == (kit_settings::get().update_via.empty() ? recommended : kit_settings::get().update_via)) selected = (int)i;
    ui::pick(first ? kit_text::str("update/method_title") : kit_text::str("tools/update_via"), labels, selected,
             [then](int i) {
                 kit_settings::get().update_via = METHODS[i];
                 ui::save_config();
                 if (then) then();
             });
}

std::string method_text()
{
    const std::string& via = kit_settings::get().update_via;
    return via.empty() ? kit_text::str("tools/update_via_values/unset")
                       : kit_text::str("tools/update_via_values/" + via);
}

void postpone(const std::string& version, std::function<void()> done)
{
    static const int DAYS[] = { 1, 7, update::SNOOZE_MAX_DAYS };
    const std::vector<std::string> labels = { kit_text::str("update/later_day"), kit_text::str("update/later_week"),
                                              kit_text::str("update/later_days", update::SNOOZE_MAX_DAYS) };
    ui::pick(kit_text::str("update/later_title"), labels, 0, [version, done](int i) {
        const std::string until = date::add_days(ui::today_date(), DAYS[i]);
        if (until.empty()) return;   // no date on the console clock: nothing to count from
        auto& cfg = kit_settings::get();
        cfg.update_snooze_version = version;
        cfg.update_snooze_until   = until;
        ui::save_config(kit_text::str("update/later_done", until));
        if (done) done();
    }, false);
}

std::string status_text(const update::Result& r, const std::string& firmware)
{
    switch (r.verdict) {
        case update::Verdict::UpdateSupports:
            return kit_text::str("fw_gate/status_supports", r.latest.version, firmware);
        case update::Verdict::UpdateNoSupport:
            return kit_text::str("fw_gate/status_no_support", r.latest.version, firmware);
        case update::Verdict::UpToDate:
        case update::Verdict::NoRelease:
            return kit_text::str("fw_gate/status_latest", app::version());
        default:
            return kit_text::str("fw_gate/status_unknown", r.error);
    }
}

std::string update_label()
{
    const std::string& via = kit_settings::get().update_via;
    if (via.empty()) return kit_text::str("update/update");
    if (via == "inplace") return kit_text::str("update/update_now");
    const launcher::Target target = launcher::find(via);
    if (target.store == launcher::Store::None || !launcher::can_launch()) return kit_text::str("update/update_manual");
    return kit_text::str("update/update_with", store_name(target.store));
}


bool pending_at_start()
{
    return update::offer_at_start(kit_settings::get().update_latest, app::version(), snooze(), ui::today_date());
}

}   // namespace update_flow
