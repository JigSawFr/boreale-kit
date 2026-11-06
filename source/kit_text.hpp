// kit_text::str — a string of the kit's own catalog (resources/i18n/<lang>/kit.json,
// the keys "kit/<key>" in Aurore's tables, merged with the app's at build).
// Every placeholder is numbered: {0} is the app's name (APP_NAME, so one text
// serves every app), {1} the app's id (APP_ID, for paths), the arguments of the call follow from {2}.
// An app that must say it its own way (the report it sends holds other
// things) gives the key "<app id>/kit/<key>" in its own catalog: that one is
// read in place, with the same placeholders.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>
#include <string>

#include "app_id.h"

namespace kit_text
{

template <typename... Args>
std::string str(const std::string& key, Args&&... args)
{
    const std::string own = APP_ID "/kit/" + key;
    return aurore::getStr(aurore::internal::getRawStr(own) == own ? "kit/" + key : own, std::string(APP_NAME), std::string(APP_ID), args...);
}

}   // namespace kit_text
