// kit_app — what the kit asks of the app it runs in: its version and where it
// comes from, the modes it is in. Declared here, defined by each app (its
// app.cpp); the app's own app.hpp includes this header and declares the rest.
// The app's names (APP_NAME, APP_ID, APP_ENV) are the other half of the
// contract: the app's app_id.h.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace app
{

std::string version();     // "1.0.0"
// The commit this build comes from ("1a2b3c4"), empty when unknown (built
// outside a git checkout). Tells which development build is installed.
std::string commit();
// The app's repository, "https://github.com/JigSawFr/<name>" (no trailing /).
const char* repo_url();
// Read-only mode: the service layer refuses every change (core/write_guard.h).
bool        read_only();
// Developer mode: the developer tools, saved in the preferences or on for this
// session only.
bool        dev_mode();

}   // namespace app
