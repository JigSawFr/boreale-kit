#!/usr/bin/env python3
"""Static checks on the layout of source/ (run in CI and by `make check`).

Errors:
- a file of the kit including something of an app: it includes only the
  kit, the standard library, Aurore, boreale-link and the vendored libraries,
  plus app_id.h (the app's names, a header each app provides). The app is
  asked through kit_app.hpp / kit_settings.hpp (which the app implements), and
  its screens through ui::app_hooks (ui/app_hooks.hpp) or Aurore's ui::hooks;
- an include going up the layers: ui/, view/ and util/ never include
  activity/ or tab/; a flow of action/ includes only the screens it opens
  itself, listed below with why;
- a C file of source/core that needs no console service (every one but the
  CONSOLE_ONLY ones below) that no host test builds (Makefile); a name below
  that is no file any more.

Usage: tools/check_sources.py [repo-root]
"""
import glob
import os
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
errors = []

INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)


def sources(folder):
    for path in sorted(glob.glob(f"{ROOT}/{folder}/**/*", recursive=True)):
        if path.endswith((".c", ".cpp", ".h", ".hpp")):
            yield os.path.relpath(path, ROOT).replace(os.sep, "/"), open(path, encoding="utf-8").read()


# ------------------------------------------------------------- the includes
# What the kit may include that is not its own: the app's names.
APP_CONTRACT = {"app_id.h"}
kit_files = {rel[len("source/"):] for rel, _ in sources("source")}
# boreale-link (library/boreale-link), the C layer the kit stands on, as on Aurore.
link_dir = os.path.join(ROOT, "library/boreale-link/source")
link_files = set(os.listdir(link_dir)) if os.path.isdir(link_dir) else set()
for rel, text in sources("source"):
    here = os.path.dirname(rel[len("source/"):])
    for inc in INCLUDE.findall(text):
        if inc in APP_CONTRACT or inc in kit_files or inc in link_files:
            continue
        # A relative include ("../core/x.h", "sim_state.h") inside the kit.
        if os.path.normpath(os.path.join(here, inc)) in kit_files:
            continue
        if inc.startswith(("Aurore", "nanovg", "yoga", "fmt/")) or inc in ("qrcodegen.hpp", "check.h"):
            continue
        errors.append(f'{rel} includes "{inc}", which is not the kit\'s: the kit includes only itself, '
                      f'{" and ".join(sorted(APP_CONTRACT))} (ask the app through kit_app.hpp, kit_settings.hpp '
                      "or ui/app_hooks.hpp)")

# --------------------------------------------------------------- layers
# Below the screens: never activity/ nor tab/.
LOWER = ["source/ui", "source/view", "source/util"]
UPWARD = re.compile(r'^\s*#\s*include\s+"((?:activity|tab)/[^"]+)"', re.M)

# A flow that opens its own screen includes it (the flow is the screen's way
# in); nothing else of action/ includes a screen.
ACTION_SCREENS = {}

for folder in LOWER:
    for rel, text in sources(folder):
        for inc in UPWARD.findall(text):
            errors.append(f'{rel} includes "{inc}": {folder}/ is below the screens (ask them through ui/app_hooks.hpp)')
seen = set()
for rel, text in sources("source/action"):
    allowed = ACTION_SCREENS.get(rel, set())
    for inc in UPWARD.findall(text):
        seen.add((rel, inc))
        if inc not in allowed:
            errors.append(f'{rel} includes "{inc}": a flow includes only a screen it opens '
                          "(else through ui/app_hooks.hpp; or list it in tools/check_sources.py, ACTION_SCREENS)")
for rel, allowed in sorted(ACTION_SCREENS.items()):
    for inc in sorted(allowed - {i for r, i in seen if r == rel}):
        errors.append(f'tools/check_sources.py: ACTION_SCREENS lists "{inc}" for {rel}, which does not include it')

# ---------------------------------------------------------- source/core
# The console's own: the simulator stands in for them on the desktop
# (source/sim/), or the desktop has no use for them.
CONSOLE_ONLY = {
    "crash_handler.c": "the console's exception handler",
    "device_key.c": "sim_device_key.cpp",
    "local_time.c": "sim_local_time.c",
    "nacp.c": "a homebrew's control data, read on the console only for now",
    "nacp_read.c": "the same",
    "platform.c": "sim_platform.cpp",
    "startup_hook.c": "libnx's userAppInit",
    "sysinfo.c": "sim_sysinfo.c",
}


def read(path):
    try:
        return open(f"{ROOT}/{path}", encoding="utf-8").read()
    except OSError:
        errors.append(f"{path} not found")
        return ""


make = set(re.findall(r"\bsource/core/(\w+\.c)\b", read("Makefile")))
core = {os.path.basename(p) for p in glob.glob(f"{ROOT}/source/core/*.c")}
for name in sorted(set(CONSOLE_ONLY)):
    if name not in core:
        errors.append(f"tools/check_sources.py names source/core/{name}, which is not there any more")
for name in sorted(core - set(CONSOLE_ONLY)):
    if name not in make:
        errors.append(f"source/core/{name} needs no console service but no host test builds it (Makefile)")

for e in errors:
    print("ERROR:", e)
print(f"sources check: {len(errors)} error(s)")
sys.exit(1 if errors else 0)
