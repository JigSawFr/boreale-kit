// sysmodule_nx — whether a sysmodule runs, and starting or stopping it now
// (pm:dmnt, pm:shell), without a reboot: what the apps' sysmodule screens and
// flow ask of the console (action/sysmodule_flow's Host). hbloader gives an
// NRO both services. Each call opens the service and closes it before it
// returns. The desktop build simulates them (source/sim/sim_sysmodule.c).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// True when a process of `tid` runs. *rc: why it could not be told (0 when
// it could).
bool   sysmodule_running(u64 tid, Result *rc);
// Starts `tid` from the SD card (Atmosphère's atmosphere/contents/<tid>).
Result sysmodule_launch(u64 tid);
// Stops it.
Result sysmodule_terminate(u64 tid);

#ifdef __cplusplus
}
#endif
