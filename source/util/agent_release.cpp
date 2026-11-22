// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/agent_release.hpp"

#include "util/json_util.hpp"
#include "util/update.hpp"

namespace agent_release
{

bool parse_manifest(const std::string& json, Manifest* out)
{
    return json_util::parse_with(json, [out](const nlohmann::json& j) {
        if (!j.is_object() || json_util::newer_schema(j, MANIFEST_SCHEMA)) return false;
        // A field missing or not of its type reads as "" or 0, which no check below takes.
        Manifest m;
        m.version = json_util::string_at(j, "version");
        m.sha256 = json_util::string_at(j, "sha256");
        m.kit = json_util::string_at(j, "kit");
        const auto p = j.find("protocol");
        if (p != j.end() && p->is_number_integer()) m.protocol = p->get<int>();
        int parts[3];
        if (!update::parse_version(m.version, parts) || m.protocol < 1) return false;
        if (m.sha256.size() != 64) return false;
        for (char c : m.sha256)
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        if (out) *out = std::move(m);
        return true;
    });
}

Verdict decide(int protocol, const std::string& installed_sha, const std::string& installed_version,
               const std::string& carried_version, const Manifest& release)
{
    // The same build is on the card: nothing to do, whatever the versions say.
    if (!installed_sha.empty() && installed_sha == release.sha256) return Verdict::UpToDate;
    for (const std::string* have : { &installed_version, &carried_version })
        if (!have->empty() && update::compare_versions(release.version, *have) <= 0) return Verdict::UpToDate;
    if (release.protocol != protocol) return Verdict::Incompatible;
    return Verdict::Newer;
}

std::string version_txt(const Manifest& release)
{
    return "version=" + release.version + "\ncommit=" + release.kit + "\nsha256=" + release.sha256 + "\n";
}

std::string manifest_url()
{
    return std::string("https://github.com/") + REPO + "/releases/latest/download/boreale-link.json";
}

std::string nsp_url(const std::string& version)
{
    return std::string("https://github.com/") + REPO + "/releases/download/v" + version + "/boreale-link.nsp";
}

}   // namespace agent_release
