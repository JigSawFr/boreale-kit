// agent_release — the shared agent as boreale-link's releases carry it: the
// manifest `boreale-link.json` (schema 1: the version, the bl:link protocol the
// agent speaks, the SHA-256 of its exefs, the kit it was built with) next to
// `boreale-link.nsp`. Reading the manifest and deciding whether the release is
// worth installing are plain C++ (host-tested, tests/agent_release); the
// network half is agent_release_net.cpp. What is taken from a release goes in the
// shape sysmodule::bundled() reads (<dir>/agent/exefs.nsp and version.txt), so
// it installs like the copy an app carries.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace agent_release
{

// The manifest's "schema" this version reads.
constexpr int MANIFEST_SCHEMA = 1;

// boreale-link's releases: where the manifest and the exefs are.
constexpr const char* REPO = "JigSawFr/boreale-link";

struct Manifest
{
    std::string version;   // "1.2.0"
    int         protocol = 0;   // the bl:link protocol the agent speaks
    std::string sha256;    // of boreale-link.nsp: 64 lower-case hex digits
    std::string kit;       // the boreale-kit commit it was built with (informative)
};

// Reads the manifest: false when a required field (version, protocol, sha256) is
// missing or malformed, or when its "schema" is newer than MANIFEST_SCHEMA (a
// missing one is 1). A field it does not know is ignored.
bool parse_manifest(const std::string& json, Manifest* out);

enum class Verdict
{
    Unknown,        // could not tell (offline, a manifest that cannot be read)
    NoRelease,      // nothing published yet
    UpToDate,       // what is installed or carried is as new as the release, or the same build
    Newer,          // the release is newer than both, and the agent speaks this build's protocol
    Incompatible,   // newer, but it speaks another protocol than this build's
};

// `protocol`: the one this build speaks (BL_RELAY_PROTOCOL). `installed_sha` and
// `installed_version`: what is on the SD card ("" when none, or put there by
// hand); `carried_version`: the copy this build carries ("" when it carries none).
Verdict decide(int protocol, const std::string& installed_sha, const std::string& installed_version,
               const std::string& carried_version, const Manifest& release);

// The version.txt that goes with a copy taken from a release, in the shape
// sysmodule::bundled() reads ("version=", "commit=", "sha256=" lines).
std::string version_txt(const Manifest& release);

// <repository>/releases/latest/download/boreale-link.json, and the exefs of a version.
std::string manifest_url();
std::string nsp_url(const std::string& version);

// The network half (agent_release_net.cpp). Both block: call them from a worker.
struct Check
{
    Verdict     verdict = Verdict::Unknown;
    Manifest    release;   // what the latest release is, when it could be read
    std::string error;     // why it could not be told (Unknown), in English
};
// Fetches the latest release's manifest and decides (see decide()). The desktop
// simulation reads <APP>_SIM_AGENT_RELEASE: "1.2.0" (a release of that version,
// speaking `protocol`), "none" (no release published) or "offline".
Check check(int protocol, const std::string& installed_sha, const std::string& installed_version,
            const std::string& carried_version);
// Fetches the exefs of `release`, checks it against the manifest's SHA-256 and puts
// it in `dir`/agent/ (exefs.nsp, then version.txt): where sysmodule::bundled()
// reads a copy. "" when it went through, else why not, in English.
std::string download(const Manifest& release, const std::string& dir);

}   // namespace agent_release
