#!/usr/bin/env python3
"""Checks the kit's string catalogs (resources/i18n/<lang>/kit.json).

Errors:
- a key used by the sources (kit_text::str("a/b") or a key-shaped literal under
  source/) that en-US lacks; a key en-US has that no source uses (a literal
  ending in "/" stands for every key under it);
- a language with other keys than en-US;
- a text that uses other arguments of the call ({2}…) than the en-US text (a
  language may leave out {0}, the app's name, and {1}, its id), or that has an
  unnumbered placeholder ("{}" or "{:d}": a kit text is always numbered, kit_text.hpp).

Usage: tools/check_resources.py [repo-root]
"""
import glob
import json
import os
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
errors = []

I18N = f"{ROOT}/resources/i18n"
langs = sorted(d for d in os.listdir(I18N) if os.path.isfile(f"{I18N}/{d}/kit.json")) if os.path.isdir(I18N) else []
if "en-US" not in langs:
    print("ERROR: resources/i18n/en-US/kit.json is missing")
    sys.exit(1)


def flat(d, pre=""):
    for k, v in d.items():
        if isinstance(v, dict):
            yield from flat(v, pre + k + "/")
        else:
            yield pre + k, v


tables = {}
for lang in langs:
    try:
        tables[lang] = dict(flat(json.load(open(f"{I18N}/{lang}/kit.json", encoding="utf-8"))))
    except (OSError, ValueError) as e:
        errors.append(f"{lang}/kit.json: {e}")
        tables[lang] = {}
en = tables["en-US"]

# --------------------------------------------------------- the sources' keys
CATEGORIES = sorted({k.split("/")[0] for k in en})
LITERAL = re.compile(r'"((?:%s)/[\w/]*)"' % "|".join(CATEGORIES))
used = set()
for path in glob.glob(f"{ROOT}/source/**/*", recursive=True):
    if path.endswith((".c", ".cpp", ".h", ".hpp")):
        used.update(LITERAL.findall(open(path, encoding="utf-8").read()))
for key in sorted(used):
    if key.endswith("/"):
        if not any(k.startswith(key) for k in en):
            errors.append(f'the sources use "{key}…", which en-US has no key under')
    elif key not in en:
        errors.append(f'the sources use "{key}", which en-US lacks')
for key in sorted(en):
    if not any(key == u or (u.endswith("/") and key.startswith(u)) for u in used):
        errors.append(f'en-US has "{key}", which no source uses')

# ---------------------------------------------------------------- languages
HOLE = re.compile(r"\{(\d*)(?::[^{}]*)?\}")


def args(text):
    """The call's arguments a text uses ({2} and up); a language may leave out
    the app's name and id ({0}, {1})."""
    return sorted({n for n in HOLE.findall(text) if n.isdigit() and int(n) >= 2})

for lang in langs:
    t = tables[lang]
    for key in sorted(set(en) - set(t)):
        errors.append(f"{lang}: lacks {key}")
    for key in sorted(set(t) - set(en)):
        errors.append(f"{lang}: has {key}, which en-US lacks")
    for key, text in sorted(t.items()):
        if any(n == "" for n in HOLE.findall(text)):
            errors.append(f'{lang}: {key} has an unnumbered placeholder: {text!r}')
        elif key in en and args(text) != args(en[key]):
            errors.append(f"{lang}: {key} has other arguments than en-US")

for e in errors:
    print("ERROR:", e)
print(f"resources check: {len(errors)} error(s), {len(en)} keys, {len(langs)} languages")
sys.exit(1 if errors else 0)
