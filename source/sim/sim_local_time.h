// sim_local_time — the simulated console's clock, when the app models one.
// By default the user clock is the host's (frozen by <APP>_SIM_NOW); an app
// whose simulated console has a clock of its own (PlayGuard's, which the screen
// can set forward) installs a function that gives it, once at start-up.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "../core/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// The function returns the user clock in POSIX seconds; NULL puts the default back.
void sim_local_time_set_now(u64 (*now)(void));

#ifdef __cplusplus
}
#endif
