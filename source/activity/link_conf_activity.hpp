// LinkConfActivity — the broker's settings the Boréale homebrews share
// (config/boreale/link.conf: the broker's address, port and TLS, who the
// console is on it, the prefixes of its topics), edited from any of them; the
// agent and every app read the same file. Each change is saved at once
// (action/link_conf_logic) and said when it cannot be. The layout is the
// app's (resources/xml/activity/link_conf.xml: a scrolling Box "lc_list", as
// agent.xml); the rows are built here and again after each change.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>
#include <functional>
#include <string>

#include "action/link_conf_logic.hpp"

class LinkConfActivity : public aurore::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/link_conf.xml");

    // What the app adds to it; both unset in the simplest app.
    struct Hooks
    {
        // Whether a change may go on now (a PIN to enter…): false stops it, the
        // app having said why. The link can change the console: PlayGuard asks.
        std::function<bool()> allowed;
        // After a change was saved: the app reads it again (its own link).
        std::function<void()> changed;
    };
    static Hooks& hooks();

    // The one line for the cell of the app's Preferences that opens this
    // screen ("mqtt.example.org:1883", "Off", "Not set"), and its colour when
    // `color` is given.
    static std::string summary(const char** color = nullptr);

    void onContentAvailable() override;

  private:
    void build();
    // Saves `next` once allowed; false when it was refused or could not be saved.
    bool commit(const BlLinkConf& next);

    BlLinkConf conf{};
    bool       newer = false;

    AURORE_BIND(aurore::Box, list, "lc_list");
};
