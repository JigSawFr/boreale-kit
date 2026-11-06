// sysinfo — firmware and Atmosphère detection (the app's own firmware policy is the app's).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// The top byte of the program ID of a forwarder's application (the HOME menu
// icon of a homebrew, as sphaira gives them).
#define SYSINFO_FWD_ID_PREFIX 0x05

// Serial number the system reports while Atmosphère blanks PRODINFO
// (ams_mitm, amsmitm_prodinfo_utils.cpp: BlankSerialNumberString).
#define SYSINFO_BLANK_SERIAL "XAW00000000000"

typedef struct {
    u32  hos_version;    // MAKEHOSVERSION layout; 0 if unknown
    bool is_atmosphere;
    bool ams_valid;
    u8   ams_major, ams_minor, ams_micro;
    bool applet_mode;    // running as a library applet (album launch): reduced memory
    bool forwarder;      // an application started from a forwarder's HOME menu icon
                         // (SYSINFO_FWD_ID_PREFIX), not over a game
    bool emummc_valid;   // spl item 65007 (ExosphereEmummcType) was read
    bool emummc;         // emuMMC active (false when unknown)
    bool blank_valid;    // spl item 65005 (ExosphereBlankProdInfo) read, or deduced from the serial
    bool blank;          // PRODINFO blanked for this boot: the system sees SYSINFO_BLANK_SERIAL
    bool serial_valid;   // set:sys GetSerialNumber succeeded
    bool device_id_valid; // spl DeviceId read (Atmosphère running)
    u64  device_id;       // names the console in its PRODINFO backups
    char serial[0x19];   // as the system reports it; NUL-terminated. Never written to reports.
} SysInfo;

typedef enum {
    SysStorage_Unknown = 0,   // Atmosphère not detected, or the emuMMC state could not be read
    SysStorage_SysMMC,        // internal NAND, Atmosphère running
    SysStorage_EmuMMC,        // emulated NAND on the SD card, Atmosphère running
} SysStorage;

void      sysinfo_get(SysInfo *out);           // cached after the first call

// The firmware's version alone: sysinfo_get's, so cached as it is (the
// simulator's reads its console each time).
static inline u32 sysinfo_hos_version(void)
{
    SysInfo info;
    sysinfo_get(&info);
    return info.hos_version;
}

static inline SysStorage sysinfo_storage(const SysInfo *info)
{
    if (!info->is_atmosphere || !info->emummc_valid) return SysStorage_Unknown;
    return info->emummc ? SysStorage_EmuMMC : SysStorage_SysMMC;
}
// "23.0.1" style string. buf must hold at least 16 bytes.
void      sysinfo_version_string(u32 version, char *buf, size_t size);

#ifdef __cplusplus
}
#endif
