<!--
The title must be a conventional commit with a scope (feat(update): …, fix(http): …):
CI checks it, and the squash merge makes it the commit subject that
release-please writes the changelog from.
-->

## ✨ At a glance

<!-- One or two sentences: what changes for someone reading the changelog. -->

## 🎯 Why

<!-- The reason, and the issue it closes, if any. The diff says what; this says what it is for. -->

## 🔧 What changes

<!-- The parts a reviewer needs to know about: the new behaviour, the contract, the layers touched. -->

<!--
For a visual change, a Before/After table with a screenshot in each cell, for example:

| Before | After |
| --- | --- |
| ![before](…) | ![after](…) |
-->

## 🧪 Checked

<!-- What CI does not run: which app it was tried in, and whether on a console or only in its desktop build. -->

## 📦 Which apps must follow

<!-- A change to the contract (kit_app.hpp, kit_settings.hpp, ui/app_hooks.hpp) or to a key: the apps that must change when they take this release. -->

## ☑️ Checklist

<!-- Only what CI does not check: CI runs the tests, the layer rules, the string catalogues and the Switch build. -->

- [ ] Nothing sent or stored in the clear: TLS verified, no secret in a plain file
