// AgentActivity — the shared agent as a Boréale app shows it: its state (on
// the SD card or not, the build the app carries, running or not) and what to
// do about it: Install or Update, Start or Stop now, Start with the console,
// Remove (action/agent_manage_flow). The layout is the app's
// (resources/xml/activity/agent.xml: a scrolling Box "ag_list", as
// diagnostic.xml); the rows are built here and again after each change.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>
#include <string>

class AgentActivity : public aurore::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/agent.xml");

    void onContentAvailable() override;

    // "Installed", "Update available"… in one or two words, and its colour
    // when `color` is given: for the cell of the app's Preferences that opens
    // this screen.
    static std::string summary(const char** color = nullptr);

  private:
    void build();

    AURORE_BIND(aurore::Box, list, "ag_list");
};
