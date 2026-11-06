// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/support_flow.hpp"

#include "activity/whats_new_activity.hpp"
#include "kit_app.hpp"
#include "kit_text.hpp"
#include "ui/app_hooks.hpp"
#include "ui/ui.hpp"
#include "kit_settings.hpp"
#include "util/support.hpp"
#include "view/funding.hpp"
#include "view/release_notes.hpp"

using namespace aurore::literals;

namespace support_flow
{
namespace
{
// Long enough for the start-up screens and questions to be on top already.
constexpr long START_DELAY_MS = 1500;

void show_reminder()
{
    // Its title, then the note and both codes.
    auto* dialog = funding::dialog(kit_text::str("about/reminder_title") + "\n\n" + kit_text::str("about/support_body"), 160);
    dialog->addButton(kit_text::str("common/later"), []() {});
    dialog->addButton(kit_text::str("about/reminder_never"), []() {
        kit_settings::get().support_reminder = false;
        ui::save_config(kit_text::str("about/reminder_off"));
    });
    dialog->open();
}
}   // namespace

void at_start(aurore::Activity* main)
{
    // Another version than the last one started: the bell says it was
    // installed (an update in place says it with the previous build).
    const std::string seen = kit_settings::get().seen_version;
    if (!seen.empty() && seen != app::version() && ui::app_hooks::get().notice_set)
        ui::app_hooks::get().notice_set("installed", kit_text::str("notices/installed", app::version()),
                                    kit_text::str("notices/updated_from", seen), kit_text::str("notices/whats_new"),
                                    []() { aurore::Application::pushActivity(new WhatsNewActivity()); }, false);

    aurore::delay(START_DELAY_MS, [main]() {
        if (!ui::on_top(main)) return;   // something else first: next start

        auto& cfg = kit_settings::get();
        const support::State state{ cfg.support_reminder, cfg.support_reminded, cfg.seen_version };
        const bool has_notes = !release_notes::current().empty() || !release_notes::highlights().empty();
        const support::Decision d = support::decide(state, app::version(), ui::today_date(), has_notes);
        // Another screen of its own came first this start (First steps, the
        // update): kept, unsaved, for the next.
        if (d.show != support::Show::Nothing && !ui::claim_start_screen("what's new or the reminder")) return;
        if (d.next.reminded != state.reminded || d.next.seen_version != state.seen_version) {
            cfg.support_reminded = d.next.reminded;
            cfg.seen_version     = d.next.seen_version;
            ui::save_config();
        }
        if (d.show == support::Show::WhatsNew) aurore::Application::pushActivity(new WhatsNewActivity());
        else if (d.show == support::Show::Reminder) show_reminder();
    });
}

}   // namespace support_flow
