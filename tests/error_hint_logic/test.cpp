// Host tests for source/action/error_hint_logic.cpp: which error codes get a
// hint, and which are shown alone.
#include "check.h"
#include <cstring>

#include "action/error_hint_logic.hpp"
#include "core/nx_types.h"

using error_hint_logic::hint_key;

static bool is(uint32_t rc, const char* key)
{
    return std::strcmp(hint_key(rc), key) == 0;
}

static void test_own()
{
    // Every result the app defines has its own hint.
    CHECK(is(NXM_RC_READ_ONLY, "read_only"));
    CHECK(is(NXM_RC_WRITE_GATED, "write_gated"));
    CHECK(is(NXM_RC_FW_UNSUPPORTED, "fw_unsupported"));
    CHECK(is(NXM_RC_UNLOCK_NOT_EFFECTIVE, "unlock_not_effective"));
    CHECK(is(NXM_RC_NOT_CUSTOM, "not_custom"));
    CHECK(is(NXM_RC_INVALID_ARGUMENT, "invalid_argument"));
    CHECK(is(NXM_RC_AUTOCORRECT_OFF, "autocorrect_off"));
    CHECK(is(NXM_RC_STATE_UNKNOWN, "state_unknown"));
    CHECK(is(NXM_RC_NOT_CONFIRMED, "not_confirmed"));
    CHECK(is(NXM_RC_NO_PIN, "no_pin"));
    CHECK(is(NXM_RC_RELOCK_FAILED, "relock_failed"));
    CHECK(is(NXM_RC_NOT_APPLIED, "not_applied"));
    CHECK(is(NXM_RC_NOT_SAVED, "not_saved"));
    CHECK(is(NXM_RC_PT_NOT_UNDERSTOOD, "pt_not_understood"));
    CHECK(is(NXM_RC_CANCELLED, "cancelled"));
    // One a newer build would add: nothing, not another one's hint.
    CHECK(is(NXM_RESULT(16), ""));
    CHECK(is(NXM_RESULT(0), ""));
    CHECK(is(NXM_RESULT(0x1FFF), ""));
}

static void test_system()
{
    CHECK(is(0x4B210, "corrupt_data"));   // 2016-0601
    CHECK(is(0x4B410, "corrupt_data"));   // 2016-0602
    CHECK(is(0x4B010, ""));               // 2016-0600: not documented
    CHECK(is(0xF601, "session_closed"));
    CHECK(is(0xF80E, "bad_pin_format"));
    CHECK(is(0xF08E, "wrong_pin"));                    // 2142-0120
    CHECK(is(0x1088E, "no_pin"));                      // 2142-0132
    CHECK(is(0x10E8E, "pctl_not_ready"));              // 2142-0135
    CHECK(is((142 | (223 << 9)), "no_internet"));       // 2142-0223
    CHECK(is(0xCC74, "clock_not_set"));                 // 2116-0102
    CHECK(is((116 | (103 << 9)), "clock_not_set"));     // 2116-0103
    CHECK(is((21 | (8 << 9)), "service_denied"));       // 2021-0008
    CHECK(is((178 | (150 << 9)), "pdm_unavailable"));   // 2178-0150
    // Any other pctl code: the module's hint.
    CHECK(is((142 | (100 << 9)), "pctl_refused"));
    CHECK(is(0x1188E, "pctl_refused"));   // 1406, cause unknown (docs/parental-controls.md)
    // Meaning unknown, or not one the app acts on: the code alone.
    CHECK(is(0x0C15, ""));                // sm InvalidServiceName
    CHECK(is((178 | (200 << 9)), ""));    // pdm "File storage failure"
    CHECK(is((116 | (1 << 9)), ""));      // time NoCapability
    CHECK(is(0xE01, ""));                 // kernel OutOfSessions
    CHECK(is(0x4B59, ""));                // libnx IncompatSysVer
    CHECK(is(0, ""));
}

int main()
{
    test_own();
    test_system();
    return CHECK_DONE("error_hint_logic assertions passed");
}
