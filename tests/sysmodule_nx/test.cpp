// Host tests for source/sim/sim_sysmodule.c, the desktop build's stand-in for
// core/sysmodule_nx.h: a module listed in <APP>_SIM_MODULES starts running, one
// starts only when its exefs.nsp is on the simulated SD card, and stopping it
// is remembered.
#include "check.h"

#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include "core/sysmodule_nx.h"
}

static const unsigned long long AGENT = 0x4200000000504741ULL;
static const unsigned long long OTHER = 0x4200000000505247ULL;

int main()
{
    char dir[] = "/tmp/sysmodule_nx_XXXXXX";
    REQUIRE(mkdtemp(dir) != nullptr);
    REQUIRE(chdir(dir) == 0);
    // Read once, at the first call.
    setenv("KITTEST_SIM_MODULES", "agent:running,0x4200000000505247:stopped", 1);

    Result rc = 1;
    CHECK(sysmodule_running(AGENT, &rc) && rc == 0);   // listed as running
    CHECK(!sysmodule_running(OTHER, nullptr));         // listed as stopped
    CHECK(!sysmodule_running(0x1234, nullptr));        // unknown

    CHECK(sysmodule_terminate(AGENT) == 0 && !sysmodule_running(AGENT, nullptr));

    // Not installed: it does not start, the console's "program not found".
    CHECK(sysmodule_launch(AGENT) == 0x0610 && !sysmodule_running(AGENT, nullptr));

    // Installed (an exefs.nsp on the simulated SD card): it starts.
    const std::string base = std::string(dir) + "/kittest_data/sd/atmosphere/contents/4200000000504741";
    REQUIRE(system(("mkdir -p " + base).c_str()) == 0);
    REQUIRE(system(("touch " + base + "/exefs.nsp").c_str()) == 0);
    CHECK(sysmodule_launch(AGENT) == 0 && sysmodule_running(AGENT, nullptr));
    CHECK(sysmodule_terminate(AGENT) == 0 && !sysmodule_running(AGENT, nullptr));

    CHECK_DONE("sysmodule_nx simulator: listed, launched only when installed, stopped");
}
