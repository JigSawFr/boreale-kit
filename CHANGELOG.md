# Changelog

## 0.3.0 (2026-11-06)

### 🚀 Features

* **kit:** the screens and flows PlayShelf and PlayGuard had twice, now shared: `ui/`, `view/`, `activity/` and `action/` (update, rollback, cleanup, crypto, dev builds, support, upload, diagnostic, changelog, what's new, funding, release notes, error hints), with `ui::app_hooks` and `rollback_flow::hooks()` for what differs per app (boreale-kit#10)
* **kit:** `kit_text.hpp` and `kit_settings.hpp`, the string helper and the `Settings` base struct, plus the kit string catalogs in 15 languages, overridable per app under `<app id>/kit/<key>` (boreale-kit#10)

### 🔧 Build

* **ci:** `tools/check_resources.py` checks the catalogs and runs in CI (boreale-kit#10)

## 0.2.0 (2026-11-06)

### 🚀 Features

* **sim:** let an app give the simulated user clock, `sim_local_time_set_now()` (boreale-kit#8)

## 0.1.0 (2026-11-06)

First release: the foundations of the kit.

### 🚀 Features

* **kit:** `util/`, `core/` and `sim/` with their host tests, taken from PlayShelf and PlayGuard (the superset of the two), and `kit_app.hpp`, the contract with the app (boreale-kit#4)

### 🔧 Build

* **ci:** the repository's tests, CI, layer rules and release-please (boreale-kit#3)
