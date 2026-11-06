// Simulated core/sysinfo.h: the console the desktop build pretends to be,
// set by <APP>_SIM_* variables (core/sim_env.h).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/sim_env.h"
#include "../core/sysinfo.h"
#include "sim_sysinfo.h"
#include "app_id.h"

static u32 (*s_hos)(void);

void sim_sysinfo_set_hos(u32 (*hos)(void)) { s_hos = hos; }

void sysinfo_get(SysInfo *out)
{
    memset(out, 0, sizeof(*out));
    // <APP>_SIM_FW=22.1.0, default 23.0.1.
    unsigned major = 23, minor = 0, micro = 1;
    const char *fw = sim_env(APP_ENV "_SIM_FW");
    if (fw) sscanf(fw, "%u.%u.%u", &major, &minor, &micro);
    out->hos_version = s_hos ? s_hos() : MAKEHOSVERSION(major, minor, micro);
    out->is_atmosphere = !sim_env_flag(APP_ENV "_SIM_NO_CFW");
    out->ams_valid = out->is_atmosphere;
    out->ams_major = 1; out->ams_minor = 12; out->ams_micro = 0;
    out->emummc_valid = out->is_atmosphere;
    out->emummc = sim_env_flag(APP_ENV "_SIM_EMUMMC");
    out->blank_valid = true;
    out->blank = sim_env_flag(APP_ENV "_SIM_BLANK");
    out->applet_mode = sim_env_flag(APP_ENV "_SIM_APPLET");
    out->forwarder = !out->applet_mode && sim_env_flag(APP_ENV "_SIM_FROM_FORWARDER");
    out->serial_valid = true;
    out->device_id_valid = true;
    out->device_id = 0x0010000000000001ULL;
    snprintf(out->serial, sizeof(out->serial), "%s", out->blank ? SYSINFO_BLANK_SERIAL : "XAW10000000001");
}

void sysinfo_version_string(u32 v, char *buf, size_t size)
{
    if (!v) { snprintf(buf, size, "?"); return; }
    snprintf(buf, size, "%u.%u.%u", (unsigned)HOSVER_MAJOR(v), (unsigned)HOSVER_MINOR(v), (unsigned)HOSVER_MICRO(v));
}
