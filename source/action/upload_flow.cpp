// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/upload_flow.hpp"

#include <aurore.hpp>
#include <ctime>
#include <format>
#include <memory>
#include <vector>

#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/util/text.hpp>

#include "action/github_login_flow.hpp"
#include "kit_app.hpp"
#include "kit_text.hpp"
#include "ui/app_hooks.hpp"
#include "ui/ui.hpp"
#include "kit_settings.hpp"
#include "util/diagnostics.hpp"
#include "util/github_auth.hpp"
#include "util/log_files.hpp"
#include "util/log_upload.hpp"
#include "util/paths.hpp"
#include "util/session_log.hpp"

using namespace aurore::literals;

namespace upload_flow
{

namespace
{
bool s_sending = false;      // one upload at a time
bool s_collecting = false;   // the files of a report being read

// The parts, shared by the dialogs and the tasks they lead to: a report with
// its files is a few hundred KB, read once and never copied after.
using Parts = std::shared_ptr<const std::vector<log_upload::Part>>;

// What goes: a diagnostic report (its link with the bug-report form, kept in
// the notifications) or a list (its link alone).
struct Kind
{
    bool        report = true;
    std::string what = "diagnostic report";   // the gist's description, in English
};

// Keeps the link in a file of its own, logs/uploads/upload_<date>.url
// (util/log_files.hpp): an Internet shortcut, so one can be found again after
// the dialog is closed, and opened from a computer.
void remember(const log_upload::Result& r)
{
    log_files::save(paths::uploads_dir(), log_files::UPLOAD, log_files::shortcut(r.url, "", ui::now_stamp()),
                    std::time(nullptr));
}

void show_link(const log_upload::Result& r, bool report)
{
    if (!report) {
        aurore::Box* code = cadran::kit::qr(r.url, 220, kit_text::str("upload/qr_list"), log_upload::link_caption(r.url), 300);
        code->setAlignSelf(aurore::AlignSelf::CENTER);
        auto* dialog = ui::dialog(kit_text::str("upload/done_list"), code);
        dialog->addButton("hints/ok"_i18n, []() {});
        dialog->open();
        return;
    }
    SysInfo si;
    sysinfo_get(&si);
    const std::string fw = ui::hos_text(si.hos_version);
    log_upload::IssueFields fields;
    fields.version = app::version();
    fields.firmware = fw;
    if (si.ams_valid) fields.atmosphere = std::format("{}.{}.{}", si.ams_major, si.ams_minor, si.ams_micro);
    // The menu it was started from is not known: the reporter adds it.
    fields.launcher = si.applet_mode ? "from the Album (applet)"
                    : si.forwarder ? "from its HOME menu icon (forwarder)"
                                   : "over a game (title override)";
    const SysStorage storage = sysinfo_storage(&si);
    if (const char* name = diagnostic::storage_name(storage)) fields.storage = name;
    const std::string issue = log_upload::issue_url(app::repo_url(), r.url, fields);

    // The link once, under its code: the text says which code is for whom,
    // where the link is found again and how the report is deleted.
    auto* row = new aurore::Box(aurore::Axis::ROW);
    row->setJustifyContent(aurore::JustifyContent::SPACE_EVENLY);
    row->addView(cadran::kit::qr(r.url, 200, kit_text::str("upload/qr_report"), log_upload::link_caption(r.url), 300));
    row->addView(cadran::kit::qr(issue, 200, kit_text::str("upload/qr_issue"), kit_text::str("upload/qr_issue_note"), 300));
    auto* dialog = ui::dialog(kit_text::str("upload/done"), row);
    dialog->addButton("hints/ok"_i18n, []() {});
    dialog->open();
}

// To a secret gist in the GitHub account the app is signed in to.
void send_gist(const Parts& parts, const Kind& kind)
{
    // Too large (a long history): the start, with the report, is what matters.
    bool cut = false;
    const Parts files = std::make_shared<const std::vector<log_upload::Part>>(
        log_upload::gist_files(*parts, log_upload::GIST_MAX_BYTES, &cut));
    std::string list;
    for (const auto& p : *parts) list += "\n• " + p.name;
    if (cut) list += "\n" + kit_text::str("upload/partial");
    const int kb = (int)((log_upload::total_bytes(*files) + 1023) / 1024);
    std::string body = kit_text::str("upload/confirm", kit_text::str("upload/where_gist"), kb) + "\n" +
                       list + "\n\n" + kit_text::str("upload/confirm_gist");
    // Its link goes into a bug report, which anyone can read.
    if (kind.report) body += " " + kit_text::str("upload/confirm_public");
    const std::string token = github_auth::token();
    ui::confirm(body, kit_text::str("upload/send"), [parts, files, token, kind]() {
        if (s_sending) return;
        s_sending = true;
        ui::notify(kit_text::str("upload/sending"));
        ui::in_background("gist upload", [parts, files, token, kind]() {
            const log_upload::Result r = log_upload::upload_gist(*files, token, app::version(), kind.what);
            aurore::sync([parts, r, kind]() {
                s_sending = false;
                if (r.relogin) {
                    // The sign-in does not allow gists (or was revoked, and
                    // then forgotten): signing in again replaces it, then the
                    // same files go. A 403 or 404 keeps it (a 403 can also be
                    // a rate limit): the GitHub account cell signs out.
                    const std::string where = kit_text::str("tabs/about") + " › " + kit_text::str("prefs/help") +
                                              " › " + kit_text::str("github/account");
                    ui::confirm(kit_text::str("upload/relogin", where), kit_text::str("upload/sign_in"),
                                [parts, kind]() {
                                    github_login_flow::sign_in([parts, kind]() { send_gist(parts, kind); });
                                });
                    return;
                }
                if (!r.ok) {
                    ui::error(kit_text::str(kind.report ? "upload/failed" : "upload/failed_list",
                                          r.error));
                    return;
                }
                remember(r);
                show_link(r, kind.report);
                // A report's link stays in the app's notifications, for later.
                if (!kind.report || !ui::app_hooks::get().notice_set) return;
                ui::app_hooks::get().notice_set("upload", kit_text::str("notices/upload"), text::short_url(r.url),
                                            kit_text::str("notices/upload_show"), [r]() { show_link(r, true); }, false);
            });
        });
    });
}

// Signed out of GitHub: signing in comes first, then the report goes.
void send(const Parts& parts, const Kind& kind = Kind{})
{
    if (!github_auth::token().empty()) {
        send_gist(parts, kind);
        return;
    }
    ui::confirm(kind.report ? kit_text::str("upload/needs_github") : kit_text::str("upload/needs_github_list"),
                kit_text::str("upload/sign_in"), [parts, kind]() {
                    github_login_flow::sign_in([parts, kind]() { send_gist(parts, kind); });
                });
}

Parts parts_of(std::vector<log_upload::Part> parts)
{
    return std::make_shared<const std::vector<log_upload::Part>>(std::move(parts));
}

// The report, the latest crashes (the app's, then Atmosphère's), the last
// runs' session logs (before the debug files, so a cut takes the history
// first) and the debug files. Read off the UI thread: the files are on the
// SD card (only the part of each that is sent is read), then the sending
// goes on, on the UI thread.
void with_debug_files(const std::string& report)
{
    if (s_collecting) return;
    s_collecting = true;
    const std::string name = kit_text::str("upload/part_report");
    // The app's own files (a recorder's): asked here, read on the worker.
    const auto& extra = ui::app_hooks::get().report_files;
    const std::function<ui::app_hooks::Hooks::ReportFiles()> more = extra ? extra() : nullptr;
    ui::in_background("report files", [name, report, more]() {
        std::vector<log_upload::Part> parts{ { name, report } };
        session_log::flush();   // this run's lines still in memory, into its file
        ui::settle_writes();    // the files as they are once the writes asked for are done
        for (auto& f : log_upload::crash_files()) parts.push_back(std::move(f));
        for (auto& f : log_upload::atmosphere_crashes(std::time(nullptr))) parts.push_back(std::move(f));
        for (auto& f : log_upload::session_files()) parts.push_back(std::move(f));
        for (auto& f : log_upload::debug_files()) parts.push_back(std::move(f));
        if (more) for (auto& f : more()) parts.push_back(std::move(f));
        if (ui::quitting()) return;
        const Parts all = parts_of(std::move(parts));
        aurore::sync([all]() {
            s_collecting = false;
            send(all);
        });
    });
}
}   // namespace

void send_report(const std::string& report)
{
    with_debug_files(report);
}

void send_list(const std::string& name, const std::string& text, const std::string& what)
{
    Kind kind;
    kind.report = false;
    kind.what = what;
    send(parts_of({ { name, text } }), kind);
}

void choose()
{
    const std::vector<std::string> saved = log_upload::saved_reports();
    if (saved.empty()) {
        send_report(diagnostic::current_report());
        return;
    }
    std::vector<std::string> labels{ kit_text::str("upload/current") };
    for (const auto& name : saved) labels.push_back(kit_text::str("upload/saved", name));
    ui::pick(kit_text::str("upload/pick"), labels, 0, [saved](int i) {
        if (i == 0) {
            send_report(diagnostic::current_report());
            return;
        }
        const std::string name = saved[(size_t)i - 1];
        std::string content;
        if (!paths::read_file(paths::reports_dir() + "/" + name, content)) {
            ui::error(kit_text::str("upload/failed", "cannot read " + name));
            return;
        }
        send(parts_of({ { "logs/reports/" + name, std::move(content) } }));
    }, false);
}

}   // namespace upload_flow
