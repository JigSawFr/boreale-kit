<div align="center">

<img src=".github/readme/banner.png" alt="boreale-kit" width="100%">

</div>

The C++23 library the Boréale apps share: network, updates, development
builds, logs, GitHub, diagnostics, and the screens and flows they have in
common.

## Using it

The kit is a submodule of each app, `library/boreale-kit`, pinned on a
release tag (`vX.Y.Z`), never on a commit of `main`. Its `source/` is added
to the app's include path and its files to the app's build, with the app's
own toolchain and flags (the Switch's, the desktop's). The kit asks the app
for its identity and settings through `kit_app.hpp` and `kit_settings.hpp`
(which the app implements), for its names through the app's `app_id.h`, and
for its screens through `ui/app_hooks.hpp`.

`make test` runs the host tests (gcc or clang, with the sanitizers);
`make check` adds the layer rules (`tools/check_sources.py`).
