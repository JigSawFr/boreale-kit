// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/link_conf_activity.hpp"

#include <cstdio>
#include <vector>

#include "kit_text.hpp"
#include "ui/ui.hpp"
#include "util/paths.hpp"

namespace
{
const BlLinkMqtt VERSIONS[] = { BlLinkMqtt_Auto, BlLinkMqtt_V5, BlLinkMqtt_V311 };

template <size_t N>
void set_text(char (&out)[N], const std::string& text)
{
    std::snprintf(out, N, "%s", text.c_str());
}

std::string version_text(BlLinkMqtt v)
{
    return kit_text::str(v == BlLinkMqtt_V5 ? "link/mqtt_v5" : v == BlLinkMqtt_V311 ? "link/mqtt_v311" : "link/mqtt_auto");
}

// A row: its title, what it is worth, A to act.
aurore::DetailCell* row(const std::string& title, const std::string& detail, std::function<void()> act)
{
    auto* cell = new aurore::DetailCell();
    cell->setText(title);
    cell->detail->setThemeColor("textColor", "cadran/text2");
    cell->setDetailText(detail);
    if (act)
        cell->registerClickAction([act](aurore::View*) {
            act();
            return true;
        });
    return cell;
}

std::string shown(const char* value, const char* when_empty)
{
    return value[0] ? std::string(value) : kit_text::str(when_empty);
}
}   // namespace

LinkConfActivity::Hooks& LinkConfActivity::hooks()
{
    static Hooks h;
    return h;
}

std::string LinkConfActivity::summary(const char** color)
{
    const auto s = link_conf_logic::summarize(link_conf_logic::load(paths::sd_root()).conf);
    if (color) *color = s.key == "link/summary/on" ? "cadran/teal" : "cadran/text2";
    return s.arg.empty() ? kit_text::str(s.key) : kit_text::str(s.key, s.arg);
}

void LinkConfActivity::onContentAvailable()
{
    this->build();
}

bool LinkConfActivity::commit(const BlLinkConf& next)
{
    if (this->newer) {
        ui::info(kit_text::str("link/newer"));
        return false;
    }
    if (hooks().allowed && !hooks().allowed()) return false;
    std::string error;
    if (!link_conf_logic::save(paths::sd_root(), next, &error)) {
        ui::error(kit_text::str("link/error"), error);
        return false;
    }
    if (hooks().changed) hooks().changed();
    this->build();
    return true;
}

void LinkConfActivity::build()
{
    const auto loaded = link_conf_logic::load(paths::sd_root());
    this->conf = loaded.conf;
    this->newer = loaded.newer;
    this->list->clearViews(true);
    const BlLinkConf c = this->conf;

    auto* header = new aurore::Header();
    header->setTitle(kit_text::str("link/title"));
    header->setMarginBottom(8);
    this->list->addView(header);

    auto* about = new aurore::Label();
    about->setText(kit_text::str(this->newer ? "link/newer" : "link/about", link_conf_logic::file(paths::sd_root())));
    about->setSingleLine(false);
    about->setFontSize(18);
    about->setMarginBottom(16);
    this->list->addView(about);

    // A text, kept when it passes `check` (a catalog key under "link/invalid/"
    // says why not), and the screen shows it.
    const auto ask = [this](const char* title_key, const char* current, int max_len,
                               std::function<std::string(const std::string&)> check,
                               std::function<void(BlLinkConf&, const std::string&)> apply) {
        ui::prompt_text(kit_text::str(title_key), current, max_len, [this, check, apply](std::string text) {
            if (const std::string why = check(text); !why.empty()) {
                ui::error(kit_text::str("link/invalid/" + why));
                return;
            }
            BlLinkConf next = this->conf;
            apply(next, text);
            this->commit(next);
        });
    };

    auto* enabled = new aurore::BooleanCell();
    enabled->init(kit_text::str("link/enabled"), c.enabled, [this](bool on) {
        BlLinkConf next = this->conf;
        next.enabled = on;
        if (!this->commit(next)) this->build();   // refused: the switch goes back
    });
    this->list->addView(enabled);

    this->list->addView(row(kit_text::str("link/host"), shown(c.host, "link/none"), [ask, c]() {
        ask("link/host_prompt", c.host, BL_LINK_HOST_MAX - 1, link_conf_logic::check_host,
            [](BlLinkConf& n, const std::string& t) { set_text(n.host, t); });
    }));
    this->list->addView(row(kit_text::str("link/port"), std::to_string(c.port), [ask, c]() {
        ask("link/port_prompt", std::to_string(c.port).c_str(), 5,
            [](const std::string& t) { return link_conf_logic::check_port(t, nullptr); },
            [](BlLinkConf& n, const std::string& t) {
                int port = n.port;
                link_conf_logic::check_port(t, &port);
                link_conf_logic::apply_port(n, port);
            });
    }));

    auto* tls = new aurore::BooleanCell();
    tls->init(kit_text::str("link/tls"), c.tls, [this](bool on) {
        BlLinkConf next = this->conf;
        next.tls = on;
        if (!this->commit(next)) this->build();
    });
    this->list->addView(tls);

    this->list->addView(row(kit_text::str("link/user"), c.allow_anonymous && !c.username[0] ? kit_text::str("link/anonymous")
                                                                                             : shown(c.username, "link/none"),
                            [this, ask, c]() {
        const std::vector<std::string> options = { kit_text::str("link/user_enter"), kit_text::str("link/user_anonymous") };
        ui::pick(kit_text::str("link/user"), options, 0, [this, ask, c](int index) {
            if (index == 1) {
                BlLinkConf next = this->conf;
                next.username[0] = '\0';
                next.password[0] = '\0';
                next.allow_anonymous = true;
                this->commit(next);
                return;
            }
            ask("link/user_prompt", c.username, BL_LINK_USER_MAX - 1,
                [](const std::string& t) { return t.empty() ? std::string("user_empty") : std::string(); },
                [](BlLinkConf& n, const std::string& t) {
                    set_text(n.username, t);
                    n.allow_anonymous = false;
                });
        });
    }));
    this->list->addView(row(kit_text::str("link/password"),
                            kit_text::str(c.password[0] ? "link/password_set" : "link/none"), [this, ask]() {
        const std::vector<std::string> options = { kit_text::str("link/password_enter"), kit_text::str("link/password_none") };
        ui::pick(kit_text::str("link/password"), options, 0, [this, ask](int index) {
            if (index == 1) {
                BlLinkConf next = this->conf;
                next.password[0] = '\0';
                this->commit(next);
                return;
            }
            ask("link/password_prompt", "", BL_LINK_PASS_MAX - 1, [](const std::string&) { return std::string(); },
                [](BlLinkConf& n, const std::string& t) { set_text(n.password, t); });
        });
    }));

    this->list->addView(row(kit_text::str("link/mqtt_version"), version_text(c.mqtt_version), [this, c]() {
        std::vector<std::string> labels;
        int selected = 0;
        for (size_t i = 0; i < 3; i++) {
            labels.push_back(version_text(VERSIONS[i]));
            if (VERSIONS[i] == c.mqtt_version) selected = (int)i;
        }
        ui::pick(kit_text::str("link/mqtt_version"), labels, selected, [this](int index) {
            BlLinkConf next = this->conf;
            next.mqtt_version = VERSIONS[index];
            this->commit(next);
        });
    }));

    this->list->addView(row(kit_text::str("link/console_name"), shown(c.console_name, "link/console_default"), [ask, c]() {
        ask("link/console_name_prompt", c.console_name, BL_LINK_NAME_MAX - 1, link_conf_logic::check_name,
            [](BlLinkConf& n, const std::string& t) { set_text(n.console_name, t); });
    }));
    this->list->addView(row(kit_text::str("link/topic_prefix"), c.topic_prefix, [ask, c]() {
        ask("link/topic_prefix_prompt", c.topic_prefix, BL_LINK_PREFIX_MAX - 1, link_conf_logic::check_prefix,
            [](BlLinkConf& n, const std::string& t) { set_text(n.topic_prefix, t); });
    }));
    this->list->addView(row(kit_text::str("link/discovery_prefix"), c.discovery_prefix, [ask, c]() {
        ask("link/discovery_prefix_prompt", c.discovery_prefix, BL_LINK_PREFIX_MAX - 1, link_conf_logic::check_prefix,
            [](BlLinkConf& n, const std::string& t) { set_text(n.discovery_prefix, t); });
    }));
}
