// Host tests for source/sim/sim_local_time.c: the simulated user clock is the
// host's unless an app installs its own, and <APP>_SIM_NOW freezes the host's.
#include "check.h"

#include <cstdlib>

extern "C" {
#include "core/local_time.h"
#include "sim/sim_local_time.h"
}

static u64 fixed() { return 1700000000ULL; }

int main()
{
    u64 posix = 0;
    LocalTime local{};
    CHECK(time_local_now(&posix, &local));
    CHECK(posix > 1700000000ULL);   // the host's clock, later than 2023

    sim_local_time_set_now(fixed);
    CHECK(time_local_now(&posix, &local) && posix == 1700000000ULL);
    CHECK(local.year == 2023);
    char buf[32];
    time_format_local(posix, buf, sizeof(buf));
    CHECK(buf[4] == '-' && buf[7] == '-');

    sim_local_time_set_now(nullptr);   // the default back
    CHECK(time_local_now(&posix, nullptr) && posix != 1700000000ULL);
    CHECK_DONE("simulated clock assertions passed");
    return 0;
}
