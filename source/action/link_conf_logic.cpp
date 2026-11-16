// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/link_conf_logic.hpp"

#include <cctype>
#include <cstdlib>

#include "util/paths.hpp"

namespace link_conf_logic
{

std::string file(const std::string& sd_root)
{
    std::string root = sd_root;
    if (root.empty() || root.back() != '/') root += '/';
    return root + BL_LINK_CONF_FILE;
}

Loaded load(const std::string& sd_root)
{
    Loaded l;
    bl_link_conf_defaults(&l.conf);
    std::string text;
    l.found = paths::read_file(file(sd_root), text);
    if (l.found) bl_link_conf_parse(&l.conf, text.data(), text.size());
    l.newer = l.found && !bl_link_conf_may_write(&l.conf);
    return l;
}

bool save(const std::string& sd_root, const BlLinkConf& conf, std::string* error)
{
    if (!bl_link_conf_may_write(&conf)) {
        if (error) *error = "link.conf left as it is: written by a newer version";
        return false;
    }
    std::string text(4096, '\0');
    const size_t n = bl_link_conf_write(&conf, &text[0], text.size());
    if (n == 0) {
        if (error) *error = "the settings do not fit";
        return false;
    }
    text.resize(n);
    const std::string path = file(sd_root);
    paths::ensure_dir(path.substr(0, path.rfind('/')));
    return paths::atomic_write(path, text, error);
}

namespace
{
bool plain(const std::string& s)
{
    for (unsigned char c : s)
        if (c <= ' ' || c == 0x7f) return false;
    return true;
}
}   // namespace

std::string check_host(const std::string& host)
{
    if (host.empty()) return "host_empty";
    if (host.size() >= BL_LINK_HOST_MAX) return "host_long";
    if (!plain(host)) return "host_space";
    return "";
}

std::string check_port(const std::string& text, int* port)
{
    if (text.empty() || text.size() > 5) return "port";
    for (unsigned char c : text)
        if (!std::isdigit(c)) return "port";
    const int value = std::atoi(text.c_str());
    if (value < 1 || value > 65535) return "port";
    if (port) *port = value;
    return "";
}

std::string check_prefix(const std::string& prefix)
{
    if (prefix.empty()) return "prefix_empty";
    if (prefix.size() >= BL_LINK_PREFIX_MAX) return "prefix_long";
    if (!plain(prefix) || prefix.find_first_of("#+") != std::string::npos || prefix.front() == '/' || prefix.back() == '/')
        return "prefix";
    return "";
}

std::string check_name(const std::string& name)
{
    if (name.size() >= BL_LINK_NAME_MAX) return "name_long";
    return "";
}

Summary summarize(const BlLinkConf& conf)
{
    if (!conf.enabled) return { "link/summary/off", "" };
    if (!conf.host[0]) return { "link/summary/not_set", "" };
    return { "link/summary/on", std::string(conf.host) + ":" + std::to_string(conf.port) };
}

}   // namespace link_conf_logic
