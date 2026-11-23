// Host tests for action/link_conf_logic: link.conf read and written on a
// fake SD card (the keys this version does not know kept, a newer file left
// alone), and the checks on what the settings screen takes in.
#include "check.h"

#include <string>
#include <sys/stat.h>

#include "action/link_conf_logic.hpp"
#include "temp_dir.hpp"
#include "util/paths.hpp"

static bool has(const std::string& text, const char* line)
{
    return text.find(line) != std::string::npos;
}

static void test_files()
{
    TempDir tmp("link_conf_logic");
    const std::string sd = tmp.path();
    CHECK(link_conf_logic::file(sd) == sd + "/config/boreale/link.conf");

    auto none = link_conf_logic::load(sd);
    CHECK(!none.found && !none.newer && !none.conf.enabled);

    BlLinkConf c = none.conf;
    c.enabled = true;
    snprintf(c.host, sizeof(c.host), "%s", "homeassistant.example.org");
    c.port = 8883;
    c.tls = true;
    std::string err;
    CHECK(link_conf_logic::save(sd, c, &err));

    std::string text;
    REQUIRE(paths::read_file(link_conf_logic::file(sd), text));
    CHECK(has(text, "host=homeassistant.example.org\n") && has(text, "port=8883\n"));

    const auto back = link_conf_logic::load(sd);
    CHECK(back.found && !back.newer && back.conf.enabled && back.conf.tls && back.conf.port == 8883);

    // A key a later version added is kept when this one writes.
    REQUIRE(paths::atomic_write(link_conf_logic::file(sd), text + "future_key=1\n", &err));
    auto again = link_conf_logic::load(sd);
    again.conf.port = 1884;
    CHECK(link_conf_logic::save(sd, again.conf, &err));
    REQUIRE(paths::read_file(link_conf_logic::file(sd), text));
    CHECK(has(text, "future_key=1\n") && has(text, "port=1884\n"));

    // A newer file is shown but never written over.
    REQUIRE(paths::atomic_write(link_conf_logic::file(sd), "schema=99\nhost=other.example.org\n", &err));
    const auto newer = link_conf_logic::load(sd);
    CHECK(newer.found && newer.newer);
    CHECK(!link_conf_logic::save(sd, newer.conf, &err) && !err.empty());
    REQUIRE(paths::read_file(link_conf_logic::file(sd), text));
    CHECK(has(text, "schema=99\n") && has(text, "host=other.example.org\n"));
}

static void test_checks()
{
    CHECK(link_conf_logic::check_host("broker.example.org").empty());
    CHECK(link_conf_logic::check_host("192.168.1.20").empty());
    CHECK(link_conf_logic::check_host("") == "host_empty");
    CHECK(link_conf_logic::check_host("a b") == "host_space");
    CHECK(link_conf_logic::check_host(std::string(300, 'a')) == "host_long");

    int port = 0;
    CHECK(link_conf_logic::check_port("1883", &port).empty() && port == 1883);
    CHECK(link_conf_logic::check_port("65535", &port).empty() && port == 65535);
    CHECK(link_conf_logic::check_port("0", &port) == "port");
    CHECK(link_conf_logic::check_port("65536", &port) == "port");
    CHECK(link_conf_logic::check_port("18x3", &port) == "port");
    CHECK(link_conf_logic::check_port("", &port) == "port");
    CHECK(port == 65535);   // not touched by a refusal

    CHECK(link_conf_logic::check_prefix("boreale").empty());
    CHECK(link_conf_logic::check_prefix("home/boreale").empty());
    CHECK(link_conf_logic::check_prefix("") == "prefix_empty");
    CHECK(link_conf_logic::check_prefix("a#") == "prefix");
    CHECK(link_conf_logic::check_prefix("a+b") == "prefix");
    CHECK(link_conf_logic::check_prefix("/a") == "prefix");
    CHECK(link_conf_logic::check_prefix("a b") == "prefix");

    CHECK(link_conf_logic::check_name("").empty());
    CHECK(link_conf_logic::check_name("Living room").empty());
    CHECK(link_conf_logic::check_name(std::string(60, 'n')) == "name_long");
}

static void test_apply_port()
{
    BlLinkConf c;
    bl_link_conf_defaults(&c);
    c.port = 1883;
    c.tls  = false;
    link_conf_logic::apply_port(c, 8884);
    CHECK(c.port == 8884 && !c.tls);   // another port: TLS untouched
    link_conf_logic::apply_port(c, 8883);
    CHECK(c.port == 8883 && c.tls);    // 8883: TLS comes on
    c.tls = false;
    link_conf_logic::apply_port(c, 8883);
    CHECK(c.port == 8883 && !c.tls);   // already 8883: the user's choice stays
    link_conf_logic::apply_port(c, 1883);
    CHECK(c.port == 1883 && !c.tls);
    c.tls = true;
    link_conf_logic::apply_port(c, 1883);
    CHECK(c.tls);                      // leaving 8883 never switches TLS off
}

static void test_summary()
{
    BlLinkConf c;
    bl_link_conf_defaults(&c);
    CHECK(link_conf_logic::summarize(c).key == "link/summary/off");
    c.enabled = true;
    CHECK(link_conf_logic::summarize(c).key == "link/summary/not_set");
    snprintf(c.host, sizeof(c.host), "%s", "mqtt.example.org");
    c.port = 1883;
    const auto s = link_conf_logic::summarize(c);
    CHECK(s.key == "link/summary/on" && s.arg == "mqtt.example.org:1883");
}

int main()
{
    test_files();
    test_checks();
    test_apply_port();
    test_summary();
    CHECK_DONE("link_conf_logic: link.conf kept, a newer one left alone, the checks, the summary");
}
