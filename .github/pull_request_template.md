<!--
The title must be a conventional commit with a scope (feat(update): …, fix(http): …):
CI checks it, and the squash merge makes it the commit subject that
release-please writes the changelog from.
-->

## What this changes, and why

<!-- The diff says what; this says what it is for. Link the issue it closes, if any. -->

## How it was verified

<!-- Beyond CI: which app it was tried in, and whether on a console or only in its desktop build. -->

## Which apps must follow

<!-- A change to the contract (kit_app.hpp, kit_settings.hpp, ui/app_hooks.hpp) or to a key: the apps that must change when they take this release. -->

## Checklist

- [ ] `make check` passes
- [ ] New strings are in every `resources/i18n/*/kit.json` (English is fine as a placeholder)
- [ ] The kit includes nothing of an app (`tools/check_sources.py`)
- [ ] Nothing sent or stored in the clear: TLS verified, no secret in a plain file
