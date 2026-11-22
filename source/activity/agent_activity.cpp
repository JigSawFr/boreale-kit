// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/agent_activity.hpp"

#include <memory>

#include "action/agent_manage_flow.hpp"
#include "kit_text.hpp"
#include "ui/ui.hpp"
#include "util/paths.hpp"

namespace
{
using agent_manage_logic::Summary;

// A row of the screen: a title and what it is worth, A to act.
aurore::DetailCell* row(const std::string& title, const std::string& detail, std::function<void()> act,
                        const char* color = "cadran/text2")
{
    auto* cell = new aurore::DetailCell();
    cell->setText(title);
    cell->detail->setThemeColor("textColor", color);
    cell->setDetailText(detail);
    if (act)
        cell->registerClickAction([act](aurore::View*) {
            act();
            return true;
        });
    return cell;
}

const char* color_of(Summary s)
{
    return s == Summary::Update ? "cadran/amber" : s == Summary::Installed ? "cadran/teal" : "cadran/text2";
}

// The state, a line, and its detail on A: where it lives, which build is on
// the card and which one this app carries.
std::string details(const agent_manage_flow::Status& s)
{
    std::string text = kit_text::str("agent/title") + "\n";
    text += "\n" + kit_text::str("agent/where", sysmodule::dir(paths::sd_root(), agent_manage_flow::module()));
    if (s.state.installed)
        text += "\n" + kit_text::str("agent/installed_version",
                                     s.state.version.empty() ? kit_text::str("agent/by_hand") : s.state.version,
                                     agent_manage_logic::short_sha(s.state.sha256));
    text += "\n" + (s.bundle.present ? kit_text::str("agent/bundled_version", s.bundle.version,
                                                      agent_manage_logic::short_sha(s.bundle.sha256))
                                     : kit_text::str("agent/not_bundled"));
    return text;
}
}   // namespace

std::string AgentActivity::summary(const char** color)
{
    const auto s = agent_manage_flow::status();
    if (color) *color = color_of(s.summary);
    std::string text = kit_text::str(agent_manage_logic::state_key(s.summary, true).empty()
                                         ? agent_manage_logic::state_key(s.summary, false)
                                         : agent_manage_logic::state_key(s.summary, true));
    if (s.state.installed)
        text += " · " + kit_text::str(s.running ? "agent/running" : "agent/stopped_state");
    return text;
}

void AgentActivity::onContentAvailable()
{
    this->build();
}

void AgentActivity::build()
{
    this->list->clearViews(true);
    const auto s = agent_manage_flow::status();
    const auto again = [this]() { this->build(); };

    auto* header = new aurore::Header();
    header->setTitle(kit_text::str("agent/title"));
    header->setMarginBottom(8);
    this->list->addView(header);

    auto* about = new aurore::Label();
    about->setText(kit_text::str("agent/about"));
    about->setSingleLine(false);
    about->setFontSize(18);
    about->setMarginBottom(16);
    this->list->addView(about);

    std::string state = kit_text::str(agent_manage_logic::state_key(s.summary, false));
    if (s.state.installed) state += " · " + kit_text::str(s.running ? "agent/running" : "agent/stopped_state");
    this->list->addView(row(kit_text::str("agent/status"), state, []() {
        ui::info(details(agent_manage_flow::status()));
    }, color_of(s.summary)));

    if (s.summary == Summary::NotInstalled || s.summary == Summary::Update) {
        const bool update = s.summary == Summary::Update;
        this->list->addView(row(kit_text::str(update ? "agent/update" : "agent/install"),
                                s.bundle.version, [again, update]() {
            ui::confirm(kit_text::str(update ? "agent/confirm_update" : "agent/confirm_install"),
                        kit_text::str(update ? "agent/update" : "agent/install"),
                        [again]() { agent_manage_flow::install(again); }, nullptr);
        }));
    }

    // boreale-link's releases: a newer agent than the carried copy and the one on the card.
    this->list->addView(row(kit_text::str("agent/release/check"), "GitHub", [again]() {
        ui::notify(kit_text::str("agent/release/checking"));
        agent_manage_flow::check_release([again](const agent_release::Check& c) {
            using agent_release::Verdict;
            switch (c.verdict) {
                case Verdict::UpToDate: ui::notify(kit_text::str("agent/release/up_to_date")); break;
                case Verdict::NoRelease: ui::notify(kit_text::str("agent/release/no_release")); break;
                case Verdict::Incompatible: ui::info(kit_text::str("agent/release/incompatible", c.release.version)); break;
                case Verdict::Unknown: ui::error(kit_text::str("agent/release/offline", c.error)); break;
                case Verdict::Newer: {
                    const agent_release::Manifest release = c.release;
                    ui::confirm(kit_text::str("agent/release/newer", release.version),
                                kit_text::str("agent/release/install", release.version),
                                [again, release]() { agent_manage_flow::install_release(release, again); }, nullptr);
                    break;
                }
            }
        });
    }));

    if (s.state.installed) {
        this->list->addView(row(kit_text::str(s.running ? "agent/stop" : "agent/start"), "", [again, running = s.running]() {
            if (running) agent_manage_flow::stop(again);
            else agent_manage_flow::start(again);
        }));
        auto* boot = new aurore::BooleanCell();
        boot->init(kit_text::str("agent/at_boot"), s.state.at_boot, [](bool on) { agent_manage_flow::set_at_boot(on); });
        this->list->addView(boot);
        this->list->addView(row(kit_text::str("agent/remove"), "", [again]() {
            ui::confirm_danger(kit_text::str("agent/confirm_remove"), kit_text::str("agent/remove"),
                               [again]() { agent_manage_flow::remove(again); });
        }));
    }
}
