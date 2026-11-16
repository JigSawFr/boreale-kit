// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "sysmodule_nx.h"

bool sysmodule_running(u64 tid, Result *rc)
{
    Result r = pmdmntInitialize();
    if (R_FAILED(r)) {
        if (rc) *rc = r;
        return false;
    }
    u64 pid = 0;
    r = pmdmntGetProcessId(&pid, tid);   // fails when no such process runs
    pmdmntExit();
    if (rc) *rc = 0;
    return R_SUCCEEDED(r) && pid != 0;
}

Result sysmodule_launch(u64 tid)
{
    Result rc = pmshellInitialize();
    if (R_FAILED(rc)) return rc;
    const NcmProgramLocation loc = { .program_id = tid, .storageID = NcmStorageId_None };
    u64 pid = 0;
    rc = pmshellLaunchProgram(0, &loc, &pid);
    pmshellExit();
    return rc;
}

Result sysmodule_terminate(u64 tid)
{
    Result rc = pmshellInitialize();
    if (R_FAILED(rc)) return rc;
    rc = pmshellTerminateProgram(tid);
    pmshellExit();
    return rc;
}
