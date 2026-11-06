// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/rollback_flow.hpp"

#include <aurore.hpp>
#include <cstdio>

#include "action/dev_build_flow.hpp"
#include "action/upload_flow.hpp"
#include "kit_app.hpp"
#include "kit_text.hpp"
#include "core/platform.h"
#include "ui/app_hooks.hpp"
#include "ui/ui.hpp"
#include "util/launcher.hpp"
#include "util/migrations.hpp"
#include "util/paths.hpp"
#include "util/rollback.hpp"

using namespace aurore::literals;

namespace rollback_flow
{

Hooks& hooks()
{
    static Hooks h;
    return h;
}

namespace
{
std::string label_file() { return paths::data_dir() + "/previous_build.json"; }
std::string check_file() { return paths::data_dir() + "/boot_check.json"; }
// Gone back from a build: its name, and `starts` 1 when its start did not
// finish (0: asked for). Read and removed by the build gone back to.
std::string back_file() { return paths::data_dir() + "/went_back.json"; }

// The start that crashed after a version change: its build's name, kept until
// another build reaches its first screen and checks the migration (started()).
std::string recheck_file() { return paths::data_dir() + "/migration_recheck.json"; }

// The level the previous build knows: previous_build.json's, else its
// release's (builds before 1.4.0 did not write it); -1 when not known.
int previous_level()
{
    int known = -1;
    if (rollback::read_level(label_file(), &known)) return known;
    std::string label;
    const auto& release_level = hooks().release_level;
    return rollback::read_label(label_file(), &label) && release_level ? release_level(label) : -1;
}

// Whether the data is in a newer layout than the previous build knows (its
// migration level, kept at install: util/rollback.hpp layout_newer()). To be
// asked before going back: the swap sets the level to 0.
bool data_layout_newer()
{
    migrations::State state;
    if (!migrations::read(paths::migrations_file(), &state)) return false;
    const int known = previous_level();
    // A cache that is read again, or steps that undo, are no loss.
    const auto& undoable = hooks().undoable;
    return rollback::layout_newer(state.level, known) && !(undoable && undoable(state.level, known));
}

// util/migrations.hpp's lines, in the log.
void log_migration(bool warning, const std::string& line)
{
    if (warning)
        aurore::Logger::warning("rollback: {}", line);
    else
        aurore::Logger::info("rollback: {}", line);
}

// The swap, the running build's name kept for the one now previous. romfs:/
// is read from the running .nro and holds it open: released around it.
bool swap(const std::string& nro, std::string* error, bool crashed)
{
    // The previous build may write the data in an earlier layout: the
    // migrations run again when this one is back (util/migrations.hpp).
    if (hooks().going_back) hooks().going_back(previous_level(), log_migration);
    else migrations::earlier_build_next(paths::migrations_file(), log_migration);
    platform_romfs_release();
    const bool ok = rollback::swap(nro, error);
    platform_romfs_mount();
    if (!ok) return false;
    rollback::write_label(label_file(), running_label(), migrations::latest());
    std::remove(check_file().c_str());
    rollback::Check back;
    back.label  = running_label();
    back.starts = crashed ? 1 : 0;
    rollback::write_check(back_file(), back);
    return true;
}
}   // namespace

bool at_start()
{
    const std::string nro = dev_build_flow::nro_file();
    if (nro.empty()) return false;
    // An install or a swap cut short by a power loss: finished (the build
    // replaced kept as the previous one) before it is looked for.
    rollback::recover(nro);
    // This start counts: it is over once the first screen is up (started()).
    rollback::Check check;
    const bool checked = rollback::read_check(check_file(), &check);
    const int before = check.starts;
    if (checked) {
        check.starts++;
        rollback::write_check(check_file(), check);
    }
    if (!rollback::offer_at_start(checked, before, paths::file_exists(rollback::previous_file(nro)))) return false;

    std::string previous;
    if (!rollback::read_label(label_file(), &previous)) previous = "the previous build";
    // Plain ASCII, in English: Aurore (and the translations) are not up yet.
    const bool newer_layout = data_layout_newer();
    std::string text = "\n  " APP_NAME " " + running_label() +
                       "\n\n  Its last start did not finish (it may have crashed).\n\n  (A)  Go back to " + previous +
                       "\n  (B)  Start " + running_label() + "\n";
    if (newer_layout)
        text += "\n  The data is in a newer layout than " + previous +
                " knows: it may not find its settings and caches (A goes back anyway).\n";
    // The crash followed a version change: the first other build that starts
    // well checks the migration from A to Z (started()).
    if (hooks().verify_all) {
        rollback::Check crashed;
        crashed.label = running_label();
        rollback::write_check(recheck_file(), crashed);
    }
    aurore::Logger::info("rollback: offered (crashed){}", newer_layout ? ", the data is in a newer layout" : "");
    if (!platform_boot_choice(text.c_str())) return false;

    // Going back replaces the app, as Install another build does: the app's
    // gate (the PIN, when one is set; the system's screen, not the app's,
    // which may be what crashed; config and Aurore are not needed).
    if (hooks().boot_gate) {
        const std::string refusal = hooks().boot_gate(previous);
        if (!refusal.empty()) {
            aurore::Logger::info("rollback: not gone back, {}", refusal);
            platform_boot_choice(("\n  " + refusal + "\n\n  (A) / (B)  Start " + running_label() + "\n").c_str());
            return false;
        }
    }

    std::string err;
    if (!swap(nro, &err, true)) {
        platform_boot_choice(("\n  Could not go back: " + err + "\n\n  (A) / (B)  Start " APP_NAME "\n").c_str());
        return false;
    }
    aurore::Logger::info("rollback: back to {}", previous);
    if (launcher::can_launch() && launcher::launch_nro(dev_build_flow::nro_sd_path())) return true;
    platform_boot_choice(("\n  " + previous + " is back in place.\n  Close " APP_NAME " and start it again.\n\n  (A) / (B)  Close\n").c_str());
    return true;
}

void started()
{
    rollback::Check check;
    if (rollback::read_check(check_file(), &check) && !check.label.empty()) {
        std::string previous;
        if (!rollback::read_label(label_file(), &previous)) previous = "?";
        if (ui::app_hooks::get().notice_set)
            ui::app_hooks::get().notice_set("installed", kit_text::str("notices/installed", check.label),
                                        kit_text::str("notices/installed_detail", previous), "", nullptr, false);
    }
    // A start that follows a crash after a version change: this build started
    // well, so the migration from the one that crashed is checked.
    rollback::Check crashed;
    if (hooks().verify_all && rollback::read_check(recheck_file(), &crashed) &&
        rollback::recheck_due(crashed.label, running_label())) {
        std::remove(recheck_file().c_str());
        aurore::Logger::info("rollback: {} crashed after a version change, this build started well: migrations checked",
                           crashed.label);
        hooks().verify_all();
    }
    std::remove(check_file().c_str());
}

void note_went_back()
{
    rollback::Check back;
    if (!rollback::read_check(back_file(), &back)) return;
    std::remove(back_file().c_str());
    const std::string from = back.label.empty() ? "?" : back.label;
    const auto& h = ui::app_hooks::get();
    if (!h.notice_set) return;
    h.notice_set("went_back", kit_text::str("notices/went_back"),
                 kit_text::str(back.starts ? "notices/went_back_crash" : "notices/went_back_detail",
                              from, running_label()),
                 back.starts ? kit_text::str("notices/send_report") : "", []() {
                     if (ui::app_hooks::get().notice_clear) ui::app_hooks::get().notice_clear("went_back");
                     upload_flow::choose();
                 }, back.starts);
}

std::string running_label()
{
    // Plain ASCII: the start-up's text screen shows it too (at_start()).
    std::string label = app::version();
    if (app::pr() && !app::commit().empty()) label += " (#" + std::to_string(app::pr()) + ", " + app::commit() + ")";
    else if (!app::commit().empty()) label += " (" + app::commit() + ")";
    return label;
}

void installed(const std::string& installed_label)
{
    rollback::write_label(label_file(), running_label(), migrations::latest());
    rollback::Check check;
    check.label = installed_label;
    rollback::write_check(check_file(), check);
}

bool previous(std::string* label, bool* newer_layout)
{
    const std::string nro = dev_build_flow::nro_file();
    if (nro.empty() || !paths::file_exists(rollback::previous_file(nro))) return false;
    if (!rollback::read_label(label_file(), label)) *label = "?";
    if (newer_layout) *newer_layout = data_layout_newer();
    return true;
}

void go_back_now()
{
    std::string label, err;
    if (!previous(&label)) return;
    if (!swap(dev_build_flow::nro_file(), &err, false)) {
        ui::error(kit_text::str("dev_build/back_failed", err));
        return;
    }
    if (launcher::can_launch() && launcher::launch_nro(dev_build_flow::nro_sd_path())) {
        aurore::Application::quit();   // hbloader starts it as soon as this one exits
        return;
    }
    ui::info(kit_text::str("dev_build/done_manual", label));
}

}   // namespace rollback_flow
