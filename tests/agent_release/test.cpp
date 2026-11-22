// Host tests for util/agent_release: the manifest of boreale-link's release (a
// newer schema or a missing field is refused, a field it does not know is
// ignored), the decision, and the version.txt of a copy taken from a release.
#include "check.h"

#include <string>

#include "util/agent_release.hpp"

using agent_release::Manifest;
using agent_release::Verdict;

static const char* SHA_A = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
static const char* SHA_B = "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";

static std::string doc(const std::string& version, const std::string& protocol, const std::string& sha,
                       const std::string& extra = "", const std::string& schema = "\"schema\": 1, ")
{
    return "{" + schema + "\"name\": \"boreale-link\", \"version\": \"" + version + "\", \"protocol\": " + protocol +
           ", \"tid\": \"0x4200000000504741\", \"sha256\": \"" + sha + "\", \"kit\": \"abc123\"" + extra + "}";
}

static void test_parse()
{
    Manifest m;
    CHECK(agent_release::parse_manifest(doc("1.2.0", "1", SHA_A), &m));
    CHECK(m.version == "1.2.0" && m.protocol == 1 && m.sha256 == SHA_A && m.kit == "abc123");

    // No schema is schema 1; a field this version does not know changes nothing.
    CHECK(agent_release::parse_manifest(doc("1.2.0", "1", SHA_A, ", \"future\": [1, 2]", ""), &m));
    CHECK(agent_release::parse_manifest(doc("1.2.0", "1", SHA_A, ", \"future\": {\"a\": 1}"), &m));

    // A newer schema is read as absent, without a crash.
    CHECK(!agent_release::parse_manifest(doc("1.2.0", "1", SHA_A, "", "\"schema\": 2, "), &m));

    // A required field missing or malformed.
    CHECK(!agent_release::parse_manifest("", &m));
    CHECK(!agent_release::parse_manifest("[]", &m));
    CHECK(!agent_release::parse_manifest("{\"schema\": 1}", &m));
    CHECK(!agent_release::parse_manifest(doc("one", "1", SHA_A), &m));
    CHECK(!agent_release::parse_manifest(doc("1.2.0", "0", SHA_A), &m));
    CHECK(!agent_release::parse_manifest(doc("1.2.0", "\"1\"", SHA_A), &m));
    CHECK(!agent_release::parse_manifest(doc("1.2.0", "1", "abcd"), &m));
    CHECK(!agent_release::parse_manifest(doc("1.2.0", "1", std::string(64, 'G')), &m));
    CHECK(!agent_release::parse_manifest(doc("1.2.0", "1", std::string(64, 'A')), &m));   // upper case
}

static void test_decide()
{
    Manifest r;
    r.version = "1.2.0";
    r.protocol = 1;
    r.sha256 = SHA_A;

    // Nothing installed, nothing carried: the release is the one.
    CHECK(agent_release::decide(1, "", "", "", r) == Verdict::Newer);
    // Newer than what is on the card.
    CHECK(agent_release::decide(1, SHA_B, "1.1.0", "", r) == Verdict::Newer);
    // The same build on the card, whatever its version file says.
    CHECK(agent_release::decide(1, SHA_A, "", "", r) == Verdict::UpToDate);
    CHECK(agent_release::decide(1, SHA_A, "0.0.1", "0.0.1", r) == Verdict::UpToDate);
    // As new as the card's, or older.
    CHECK(agent_release::decide(1, SHA_B, "1.2.0", "", r) == Verdict::UpToDate);
    CHECK(agent_release::decide(1, SHA_B, "1.3.0", "", r) == Verdict::UpToDate);
    // As new as the copy this build carries: the carried one is offered by the update already.
    CHECK(agent_release::decide(1, SHA_B, "1.0.0", "1.2.0", r) == Verdict::UpToDate);
    CHECK(agent_release::decide(1, "", "", "1.1.0", r) == Verdict::Newer);
    // Another protocol than this build's: newer, but not for it.
    r.protocol = 2;
    CHECK(agent_release::decide(1, SHA_B, "1.1.0", "", r) == Verdict::Incompatible);
    // …and a release that is not newer is up to date whatever it speaks.
    CHECK(agent_release::decide(1, SHA_B, "1.2.0", "", r) == Verdict::UpToDate);
}

static void test_files()
{
    Manifest r;
    r.version = "1.2.0";
    r.protocol = 1;
    r.sha256 = SHA_A;
    r.kit = "abc123";
    CHECK(agent_release::version_txt(r) == std::string("version=1.2.0\ncommit=abc123\nsha256=") + SHA_A + "\n");
    CHECK(agent_release::manifest_url() == "https://github.com/JigSawFr/boreale-link/releases/latest/download/boreale-link.json");
    CHECK(agent_release::nsp_url("1.2.0") == "https://github.com/JigSawFr/boreale-link/releases/download/v1.2.0/boreale-link.nsp");
}

int main()
{
    test_parse();
    test_decide();
    test_files();
    CHECK_DONE("agent_release: the manifest read, the decision, the files of a copy from a release");
}
