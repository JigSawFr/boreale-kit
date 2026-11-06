// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/crypto_flow.hpp"
#include "kit_text.hpp"

#include <aurore.hpp>
#include <aurore/cadran/ui/ui.hpp>
#include <cstdio>

#include "ui/ui.hpp"
#include "kit_settings.hpp"
#include "util/crypto_backend.hpp"
#include "util/crypto_bench.hpp"

namespace crypto_flow
{
namespace
{
constexpr const char* NAMES[] = { "auto", "software", "hardware" };

std::string mbps(double v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", v);
    return buf;
}

// What runs one primitive: the engine, the portable code because the setting
// says so, because the engine failed its check, or because there is none.
std::string runs(const crypto::Engine* in_use, const crypto::Engine* verified, bool engine_has)
{
    if (in_use) return kit_text::str("crypto/runs_hardware");
    if (verified) return kit_text::str("crypto/runs_set_aside");
    if (engine_has) return kit_text::str("crypto/runs_failed");
    return kit_text::str("crypto/runs_software");
}

// The benchmark as the dialog shows it; the log keeps crypto::bench_text.
std::string bench_body(const std::vector<crypto::BenchRow>& rows)
{
    const crypto::Engine* hw = crypto::hardware_engine();
    std::string           out = kit_text::str("crypto/bench_title");
    out += "\n\n" + kit_text::str("crypto/runs_aes", runs(crypto::aes_engine(), crypto::detail::aes.load(), hw && hw->aes_encrypt));
    out += "\n" + kit_text::str("crypto/runs_sha", runs(crypto::sha_engine(), crypto::detail::sha.load(), hw && hw->sha256_blocks));
    out += "\n";
    for (const crypto::BenchRow& r : rows) {
        out += "\n";
        if (r.engine < 0)
            out += kit_text::str("crypto/row_software", r.name, mbps(r.software));
        else
            out += kit_text::str("crypto/row", r.name, mbps(r.software), mbps(r.engine), mbps(r.software > 0 ? r.engine / r.software : 0.0));
    }
    return out;
}
}   // namespace

void apply()
{
    static bool offered = false;
    if (!offered) {
        offered = true;
        crypto::offer_engine(crypto::hardware_engine());
    }
    crypto::set_backend(crypto::backend_from_name(kit_settings::get().crypto));
    aurore::Logger::info("{}", crypto::status());
}

std::string text()
{
    const crypto::Backend b = crypto::backend_from_name(kit_settings::get().crypto);
    const char*           key = b == crypto::Backend::Software ? "crypto/software" : b == crypto::Backend::Hardware ? "crypto/hardware" : "crypto/auto";
    return kit_text::str(crypto::aes_engine() ? "crypto/in_use_hardware" : "crypto/in_use_software", kit_text::str(key));
}

void choose(std::function<void()> done)
{
    const std::vector<std::string> labels = { kit_text::str("crypto/auto_long"), kit_text::str("crypto/software"),
                                              kit_text::str("crypto/hardware") };
    int                            now = 0;
    for (int i = 0; i < 3; i++)
        if (kit_settings::get().crypto == NAMES[i]) now = i;
    ui::pick(kit_text::str("crypto/title"), labels, now, [done](int i) {
        kit_settings::get().crypto = NAMES[i];
        ui::save_config();
        apply();
        if (done) done();
    });
}

void benchmark()
{
    ui::notify(kit_text::str("crypto/measuring"));
    ui::in_background("crypto benchmark", []() {
        const std::vector<crypto::BenchRow> rows = crypto::benchmark(std::chrono::milliseconds(1000));
        aurore::Logger::info("benchmark: {}", crypto::bench_text(rows));
        aurore::sync([rows]() { ui::info(bench_body(rows)); });
    });
}
}   // namespace crypto_flow
