// sim_sysinfo — the simulated console's firmware, when the app models one.
// By default sysinfo_get() reads <APP>_SIM_FW (default 23.0.1); an app whose
// simulated console has its own (PlayGuard's, in its state) installs a function
// that gives it, once at start-up.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "../core/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// The function returns a MAKEHOSVERSION value; NULL puts the default back.
void sim_sysinfo_set_hos(u32 (*hos)(void));

#ifdef __cplusplus
}
#endif
