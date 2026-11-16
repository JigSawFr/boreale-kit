// The desktop build's core/sysmodule_nx.h: which sysmodules "run", in memory.
// <APP>_SIM_MODULES=agent:running (comma-separated name:state, or a program
// id in hex, 0x4200000000504741:running) starts with that module running; the
// shared agent is the only name the kit knows. A module starts only when its
// exefs.nsp is on the simulated SD card (./<app>_data/sd/).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../core/sim_env.h"
#include "../core/sysmodule_nx.h"
#include "app_id.h"

#define SIM_MAX 4
#define SIM_RC_NOT_INSTALLED ((Result)0x0610)   // ldr: program not found
#define SIM_AGENT_TID 0x4200000000504741ULL

static struct {
    bool init;
    u64  running[SIM_MAX];
} M;

static u64 tid_of(const char *name, size_t len)
{
    if (len == 5 && !strncmp(name, "agent", 5)) return SIM_AGENT_TID;
    if (len > 2 && len <= 18 && name[0] == '0' && (name[1] == 'x' || name[1] == 'X')) {
        char text[19];
        memcpy(text, name, len);
        text[len] = '\0';
        return strtoull(text, NULL, 16);
    }
    return 0;
}

static void set_running(u64 tid, bool on)
{
    if (!tid) return;
    for (int i = 0; i < SIM_MAX; i++)
        if (M.running[i] == tid) {
            if (!on) M.running[i] = 0;
            return;
        }
    if (!on) return;
    for (int i = 0; i < SIM_MAX; i++)
        if (!M.running[i]) {
            M.running[i] = tid;
            return;
        }
}

static void sim_init(void)
{
    if (M.init) return;
    M.init = true;
    const char *list = sim_env(APP_ENV "_SIM_MODULES");
    while (list && *list) {
        const char *end = strchr(list, ',');
        const size_t len = end ? (size_t)(end - list) : strlen(list);
        const char *colon = memchr(list, ':', len);
        const size_t state_len = colon ? len - (size_t)(colon + 1 - list) : 0;
        if (colon && state_len == 7 && !strncmp(colon + 1, "running", 7))
            set_running(tid_of(list, (size_t)(colon - list)), true);
        list = end ? end + 1 : NULL;
    }
}

bool sysmodule_running(u64 tid, Result *rc)
{
    sim_init();
    if (rc) *rc = 0;
    for (int i = 0; i < SIM_MAX; i++)
        if (M.running[i] == tid) return true;
    return false;
}

Result sysmodule_launch(u64 tid)
{
    sim_init();
    char path[160];
    snprintf(path, sizeof(path), "./" APP_ID "_data/sd/atmosphere/contents/%016llX/exefs.nsp", (unsigned long long)tid);
    struct stat st;
    if (stat(path, &st) != 0) return SIM_RC_NOT_INSTALLED;
    set_running(tid, true);
    return 0;
}

Result sysmodule_terminate(u64 tid)
{
    sim_init();
    set_running(tid, false);
    return 0;
}
