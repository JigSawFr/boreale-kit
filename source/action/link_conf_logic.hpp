// link_conf_logic — the broker's settings the Boréale homebrews share
// (sd:/config/boreale/link.conf, boreale-link's bl_link_conf): read, checked
// and written for the kit's settings screen (activity/link_conf_activity),
// no UI and no console call: host-tested (tests/link_conf_logic). A file
// written by a newer version is read and shown but never written over, and
// the keys this version does not know are kept.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

#include "bl_link_conf.h"

namespace link_conf_logic
{

// <sd_root>/config/boreale/link.conf
std::string file(const std::string& sd_root);

struct Loaded
{
    BlLinkConf conf;
    bool       found = false;   // the file was there
    bool       newer = false;   // written by a newer version: shown, never written over
};
Loaded load(const std::string& sd_root);

// Writes `conf` (its unknown keys kept), the directory made when needed.
// False with `error` when it could not: a newer file, a settings text that
// does not fit, the card.
bool save(const std::string& sd_root, const BlLinkConf& conf, std::string* error);

// What a value the user typed is worth: "" when it can be kept, else the
// catalog key under "link/invalid/" that says why.
std::string check_host(const std::string& host);
std::string check_port(const std::string& text, int* port);
std::string check_prefix(const std::string& prefix);   // a topic prefix: no wildcard, no space
std::string check_name(const std::string& name);       // the console's name: may be empty (its own)

// The one line a cell that opens the screen shows: a catalog key under
// "kit/" and its argument ({2}), so the caller says it in its own words.
struct Summary
{
    std::string key;   // "link/summary/off", "link/summary/not_set" or "link/summary/on"
    std::string arg;   // "host:port" for "on"
};
Summary summarize(const BlLinkConf& conf);

}   // namespace link_conf_logic
