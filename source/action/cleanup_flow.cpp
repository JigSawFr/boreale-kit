// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/cleanup_flow.hpp"
#include "kit_text.hpp"

#include <aurore.hpp>
#include <functional>
#include <memory>
#include <string>

#include "ui/ui.hpp"
#include "util/data_cleanup.hpp"
#include "util/paths.hpp"
#include "util/session_log.hpp"

using namespace aurore::literals;

namespace cleanup_flow
{

namespace
{
// "6 files (2 MB)", in the app's language.
std::string files_text(size_t count, uint64_t bytes)
{
    uint64_t value = 0;
    const bool mb = data_cleanup::size_unit(bytes, &value) == data_cleanup::Unit::MB;
    const std::string size = kit_text::str(mb ? "dev/size_mb" : "dev/size_kb", std::to_string(value));
    if (count == 1) return kit_text::str("dev/files_one", size);
    return kit_text::str("dev/files", std::to_string(count), size);
}

// Lists the files on the write queue (behind the writes asked before, and
// off the UI thread: the icons are a few hundred files), then asks, then
// says what went.
void run(const char* what, std::function<data_cleanup::Plan()> make_plan, const char* body_key)
{
    ui::write_later(what, [what, make_plan, body_key]() {
        auto plan = std::make_shared<data_cleanup::Plan>(make_plan());
        aurore::sync([what, plan, body_key]() {
            if (plan->files.empty()) {
                ui::info(kit_text::str("dev/clear_empty"));
                return;
            }
            ui::confirm_danger(kit_text::str(body_key, files_text(plan->files.size(), plan->bytes)),
                               kit_text::str("common/delete"), [what, plan]() {
                ui::write_later(what, [what, plan]() {
                    const data_cleanup::Outcome out = data_cleanup::remove(*plan);
                    aurore::Logger::info("cleanup ({}): {} file(s) removed, {} bytes, {} not removed", what, out.removed,
                                       out.bytes, out.failed);
                    aurore::sync([out]() {
                        const std::string done = files_text(out.removed, out.bytes);
                        if (out.failed) ui::error(kit_text::str("dev/clear_failed", done, std::to_string(out.failed)));
                        else ui::info(kit_text::str("dev/cleared", done));
                    });
                });
            });
        });
    });
}
}   // namespace

void clear_cache()
{
    run("clear cache", []() { return data_cleanup::cache(paths::data_dir()); }, "dev/clear_cache_body");
}

void clear_logs()
{
    run("clear logs", []() { return data_cleanup::logs(paths::data_dir(), session_log::current_name()); },
        "dev/clear_logs_body");
}

}   // namespace cleanup_flow
