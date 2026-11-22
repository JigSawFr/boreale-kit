// The network half of agent_release.hpp (the rest stays plain C++ for the host tests).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>
#include <string_view>

#include "app_id.h"
#ifndef __SWITCH__
#include "sim/sim_net.hpp"
#endif
#include "util/agent_release.hpp"
#include "util/http.hpp"
#include "util/paths.hpp"
#include "util/sha256.hpp"

namespace agent_release
{

namespace
{
constexpr size_t MAX_MANIFEST = 16 * 1024;
constexpr size_t MAX_NSP = 4 * 1024 * 1024;   // an exefs is a few hundred KiB

std::string sha256_of(const std::string& bytes)
{
    Sha256 h;
    h.update(std::string_view(bytes));
    return h.hex();
}

#ifndef __SWITCH__
// The simulated release's exefs: made of its version, so its manifest can say its SHA-256.
std::string sim_body(const std::string& version)
{
    return "SIMULATED AGENT " + version;
}
#endif
}   // namespace

Check check(int protocol, const std::string& installed_sha, const std::string& installed_version,
            const std::string& carried_version)
{
    Check r;
    std::string body;
#ifndef __SWITCH__
    if (const char* sim = sim_net::value(APP_ENV "_SIM_AGENT_RELEASE")) {
        if (std::strcmp(sim, "none") == 0) {
            r.verdict = Verdict::NoRelease;
            r.error = "HTTP 404";
            return r;
        }
        if (std::strcmp(sim, "offline") == 0) {
            sim_net::offline(&r.error);
            return r;
        }
        body = "{\"schema\": 1, \"version\": \"" + std::string(sim) + "\", \"protocol\": " + std::to_string(protocol) +
               ", \"sha256\": \"" + sha256_of(sim_body(sim)) + "\", \"kit\": \"simulated\"}";
    } else
#endif
    {
        auto got = http::get(manifest_url(), MAX_MANIFEST, 10);
        if (!got) {
            r.error = got.error().error;
            // GitHub answers 404 for releases/latest while none is published.
            if (got.error().status == 404) r.verdict = Verdict::NoRelease;
            return r;
        }
        body = std::move(got->body);
    }
    if (!parse_manifest(body, &r.release)) {
        r.error = "unexpected boreale-link.json";
        return r;
    }
    r.verdict = decide(protocol, installed_sha, installed_version, carried_version, r.release);
    return r;
}

std::string download(const Manifest& release, const std::string& dir)
{
    std::string body;
#ifndef __SWITCH__
    if (sim_net::value(APP_ENV "_SIM_AGENT_RELEASE")) body = sim_body(release.version);
    else
#endif
    {
        auto got = http::get(nsp_url(release.version), MAX_NSP, 60);
        if (!got) return got.error().error;
        body = std::move(got->body);
    }
    if (sha256_of(body) != release.sha256) return "the download does not match its checksum";
    const std::string folder = dir + "/agent";
    if (!paths::ensure_dir(folder)) return "cannot create " + folder;
    std::string why;
    // The exefs first: a copy without its version.txt is not a copy (sysmodule::bundled).
    if (!paths::atomic_write(folder + "/exefs.nsp", body, &why)) return why;
    if (!paths::atomic_write(folder + "/version.txt", version_txt(release), &why)) return why;
    return "";
}

}   // namespace agent_release
