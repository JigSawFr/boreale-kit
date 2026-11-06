// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/changelog_activity.hpp"

#include "kit_app.hpp"
#include "kit_text.hpp"
#include "view/release_notes.hpp"

using namespace aurore::literals;

void ChangelogActivity::onContentAvailable()
{
    if (auto* applet = dynamic_cast<aurore::AppletFrame*>(this->getContentView()))
        applet->setTitle(kit_text::str("title") + " " + app::version());
    std::string date;
    const auto lines = release_notes::current(&date);
    header->setTitle(kit_text::str("about/section_changelog", app::version()));
    if (!date.empty()) header->setSubtitle(date);
    release_notes::fill(notes, lines);
    // Nothing else to focus: the notes take it (with no highlight), so the
    // stick and the D-pad scroll them.
    notes->setFocusable(true);
    notes->setHideHighlight(true);
    note->setSingleLine(false);
    note->setText(kit_text::str("about/changelog_note", std::string(app::repo_url()) + "/releases"));
}
