// ui — firmware and storage lines as text, the game-patch report.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/app_hooks.hpp"
#include "ui/ui.hpp"
#include "kit_text.hpp"

#include <aurore/cadran/ui/cadran_kit.hpp>

#include "util/diagnostics.hpp"
#include "util/paths.hpp"

#include <format>

using namespace aurore::literals;

namespace ui
{

std::string hos_text(u32 version)
{
    char buf[16];
    sysinfo_version_string(version, buf, sizeof(buf));
    return buf;
}

std::string fw_text(const SysInfo& info)
{
    const std::string fw = hos_text(info.hos_version);
    std::string ams = info.ams_valid ? std::format("{}.{}.{}", info.ams_major, info.ams_minor, info.ams_micro)
                                     : std::string("?");
    return std::format("{} · Atmosphère {}", fw, ams);
}

std::string storage_text(const SysInfo& info)
{
    switch (sysinfo_storage(&info)) {
        case SysStorage_EmuMMC: return kit_text::str("tools/storage_emummc");
        case SysStorage_SysMMC: return kit_text::str("tools/storage_sysmmc");
        default:                return kit_text::str("tools/storage_unknown");
    }
}

std::string storage_short(const SysInfo& info)
{
    const char* name = diagnostic::storage_name(sysinfo_storage(&info));
    return name ? name : "—";
}

namespace
{
bool            s_patches_read = false;
patches::Report s_patches;
}   // namespace

void reread_patch_report()
{
    s_patches_read = false;
}

const patches::Report& patch_report()
{
    bool& read = s_patches_read;
    patches::Report& report = s_patches;
    if (!read) {
        SysInfo si;
        sysinfo_get(&si);
        const std::string fw = hos_text(si.hos_version);
        if (const auto& prepare = app_hooks::get().before_patch_report) prepare();
        report = patches::detect(paths::sd_root(), fw, si.emummc);
        read = true;
    }
    return report;
}

}   // namespace ui
