// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/error_hint_logic.hpp"

#include "core/nx_types.h"

namespace error_hint_logic
{

const char* hint_key(uint32_t rc)
{
    if (NXM_IS_APP_RESULT(rc)) {
        // By description, whatever the number: the kit's NXM_RC_* are shared with the apps.
        switch (NXM_RESULT_DESC(rc)) {
            case NXM_RESULT_DESC(NXM_RC_READ_ONLY):            return "read_only";
            case NXM_RESULT_DESC(NXM_RC_WRITE_GATED):          return "write_gated";
            case NXM_RESULT_DESC(NXM_RC_FW_UNSUPPORTED):       return "fw_unsupported";
            case NXM_RESULT_DESC(NXM_RC_UNLOCK_NOT_EFFECTIVE): return "unlock_not_effective";
            case NXM_RESULT_DESC(NXM_RC_NOT_CUSTOM):           return "not_custom";
            case NXM_RESULT_DESC(NXM_RC_INVALID_ARGUMENT):     return "invalid_argument";
            case NXM_RESULT_DESC(NXM_RC_AUTOCORRECT_OFF):      return "autocorrect_off";
            case NXM_RESULT_DESC(NXM_RC_STATE_UNKNOWN):        return "state_unknown";
            case NXM_RESULT_DESC(NXM_RC_NOT_CONFIRMED):        return "not_confirmed";
            case NXM_RESULT_DESC(NXM_RC_NO_PIN):               return "no_pin";
            case NXM_RESULT_DESC(NXM_RC_RELOCK_FAILED):        return "relock_failed";
            case NXM_RESULT_DESC(NXM_RC_NOT_APPLIED):          return "not_applied";
            case NXM_RESULT_DESC(NXM_RC_NOT_SAVED):            return "not_saved";
            case NXM_RESULT_DESC(NXM_RC_PT_NOT_UNDERSTOOD):    return "pt_not_understood";
            case NXM_RESULT_DESC(NXM_RC_CANCELLED):            return "cancelled";
            default: return "";
        }
    }
    switch (rc) {
        case 0xF601: return "session_closed";   // 2001-0123 SessionClosed
        case 0xF80E: return "bad_pin_format";
        // The 22.5.0 pctl sysmodule (NX-Pctl-Manager's docs/pctl-reverse-engineering.md):
        // 2142-0120, the PIN check of 1201 refusing a wrong or malformed PIN;
        case 0xF08E: return "wrong_pin";
        // 2142-0132, the same check with no PIN registered (also reported
        // writing the play timer on 22.1, NX-Pctl-Manager #5);
        case 0x1088E: return "no_pin";
        // 2142-0135, its settings manager not set up yet.
        case 0x10E8E: return "pctl_not_ready";
        // 2142-0223 (switchbrew): IsAnyInternetRequestAccepted returned false, no connection.
        case 0x1BE8E: return "no_internet";
        // 2116-0102 (switchbrew: OffsetInvalid): switch-time saw it reading a never-set network clock.
        case 0xCC74: return "clock_not_set";
        // 2116-0103 (switchbrew: UninitializedClock).
        case 0xCE74: return "clock_not_set";
        // 2021-0008 (switchbrew: sm NotAllowed): no access to the service asked for.
        case 0x1015: return "service_denied";
        // 2016-0601 / 2016-0602 (Nintendo support): corrupt data when a game
        // starts; also what the integrity check answers when it finds it.
        case 0x4B210:
        case 0x4B410: return "corrupt_data";
        // 2178-0150 (switchbrew: pdm "Service unavailable").
        case 0x12CB2: return "pdm_unavailable";
        default: break;
    }
    if ((rc & 0x1FF) == 142) return "pctl_refused";   // pctl module
    return "";
}

}   // namespace error_hint_logic
