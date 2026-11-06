# boreale-kit's host tests: `make test` builds and runs every suite in
# parallel with the sanitizers, as CI does (gcc and clang). The library itself
# is built by each app from source/ (its own toolchain, flags and list of
# files); `make check` also runs the layer rules (tools/check_sources.py).
#
# A suite is three variables: <name>_SRC (its sources, the test last),
# <name>_FLAGS (the standard, defines, include paths; a C++ suite without one
# gets TEST_CXXFLAGS) and an optional <name>_LIBS; the test source's suffix
# picks the compiler. An optional <name>_CSRC (C files, with <name>_CFLAGS) is
# built by $(CC) and linked into a C++ suite; one that links util/date.cpp
# gets CALENDAR_C when it names none. Each suite's output is in
# build/host-tests/<name>.log, and <name>.ok marks a passed one.

BUILD   := build
CC      ?= gcc
CXX     ?= g++
TESTOUT := $(BUILD)/host-tests
JOBS    ?= $(shell nproc 2>/dev/null || echo 4)
SAN     ?= -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined
COV     ?=
# -Itests: check.h, the CHECK() every suite uses; -Itests/stub: the app_id.h
# of the app the tests stand in for. _GLIBCXX_ASSERTIONS: libstdc++ checks
# what the sanitizers cannot see.
CWARN   := -Wall -Wextra -Wshadow -Werror -D_GLIBCXX_ASSERTIONS $(SAN) $(COV) -Itests -Itests/stub
JSON_HPP := library/aurore/library/include/aurore/extern/nlohmann/json.hpp

.PHONY: clean test check FORCE

# Aurore's headers (nlohmann/json) as system headers: their warnings are not
# the kit's.
TEST_CXXFLAGS   := -std=c++23 -Isource -Ilibrary/boreale-link/source -isystem library/aurore/library/include
CALENDAR_C      := source/core/calendar.c
CALENDAR_CFLAGS := -std=gnu11 -Isource/core

migrations_SRC := source/util/paths.cpp source/util/date.cpp source/util/update.cpp source/util/migrations.cpp tests/migrations/test.cpp
write_guard_SRC := source/core/write_guard.c tests/write_guard/test.c
write_guard_FLAGS := -std=gnu11 -Isource/core
dev_builds_SRC := source/util/paths.cpp source/util/sha256.cpp source/util/date.cpp source/util/update.cpp library/aurore/library/lib/cadran/util/safe_text.cpp source/util/dev_builds.cpp source/util/zip_read.cpp source/util/github_auth.cpp source/util/secret_box.cpp source/util/aes.cpp source/sim/sim_device_key.cpp tests/dev_builds/test.cpp
dev_builds_LIBS := -lz
dev_builds_net_SRC := source/util/paths.cpp source/util/sha256.cpp source/util/date.cpp source/util/update.cpp library/aurore/library/lib/cadran/util/safe_text.cpp source/util/dev_builds.cpp source/util/zip_read.cpp source/util/github_auth.cpp source/util/secret_box.cpp source/util/aes.cpp source/sim/sim_device_key.cpp source/util/http_logic.cpp source/util/zip_write.cpp source/sim/sim_net.cpp source/sim/sim_env.cpp source/util/dev_builds_net.cpp tests/dev_builds_net/test.cpp
dev_builds_net_LIBS := -lz
session_log_SRC := source/util/paths.cpp source/util/log_files.cpp source/util/session_log.cpp source/util/crash_report.cpp tests/session_log/test.cpp
session_log_CFLAGS := -std=c11 -Isource/util
session_log_CSRC := source/util/crash_dump.c
diagnostics_logic_SRC := source/util/paths.cpp source/util/patches.cpp source/util/diagnostics_logic.cpp tests/diagnostics_logic/test.cpp
update_SRC := source/util/date.cpp source/util/update.cpp tests/update/test.cpp
frame_stats_SRC := source/util/paths.cpp source/util/frame_stats.cpp tests/frame_stats/test.cpp
rollback_SRC := source/util/paths.cpp source/util/rollback.cpp tests/rollback/test.cpp
date_SRC := source/util/date.cpp tests/date/test.cpp
startup_SRC := source/util/startup.c tests/startup/test.c
startup_FLAGS := -std=gnu11 -Isource/util
crash_dump_FLAGS := -std=c11 -Isource/util
crash_dump_SRC := source/util/crash_dump.c tests/crash_dump/test.c
text_util_SRC := tests/text_util/test.cpp
worker_threads_SRC := source/util/worker_threads.cpp tests/worker_threads/test.cpp
worker_threads_LIBS := -pthread
# The sealed bytes are the app's: the suite runs once under each real app_id.h.
secret_box_playshelf_SRC   := source/util/secret_box.cpp source/util/aes.cpp source/sim/sim_device_key.cpp source/util/sha256.cpp source/util/paths.cpp tests/secret_box/test.cpp
secret_box_playshelf_FLAGS := -std=c++23 -Itests/stub/playshelf -Isource -isystem library/aurore/library/include
secret_box_playguard_SRC   := source/util/secret_box.cpp source/util/aes.cpp source/sim/sim_device_key.cpp source/util/sha256.cpp source/util/paths.cpp tests/secret_box/test.cpp
secret_box_playguard_FLAGS := -std=c++23 -Itests/stub/playguard -Isource -isystem library/aurore/library/include
data_cleanup_SRC := source/util/data_cleanup.cpp tests/data_cleanup/test.cpp
launcher_CFLAGS := -std=gnu11 -Ilibrary/boreale-link/source -Isource/core
launcher_SRC := source/util/paths.cpp source/util/launcher.cpp tests/launcher/test.cpp
sphaira_entry_SRC := source/util/paths.cpp source/util/sphaira_entry.cpp tests/sphaira_entry/test.cpp
launcher_CSRC := library/boreale-link/source/bl_kv.c library/boreale-link/source/bl_json.c library/boreale-link/source/bl_jread.c library/boreale-link/source/bl_app.c library/boreale-link/source/bl_notices.c library/boreale-link/source/bl_updates.c source/core/calendar.c
http_logic_SRC := source/util/http_logic.cpp tests/http_logic/test.cpp
changelog_SRC := source/util/date.cpp source/util/changelog.cpp source/util/update.cpp tests/changelog/test.cpp
perf_SRC := source/util/perf.cpp tests/perf/test.cpp
crypto_SRC := source/util/crypto_backend.cpp source/util/crypto_bench.cpp source/util/crypto_hw.cpp source/util/aes.cpp source/util/sha256.cpp tests/crypto/test.cpp
log_files_SRC := source/util/paths.cpp source/util/log_files.cpp tests/log_files/test.cpp
log_upload_SRC := source/util/paths.cpp source/util/log_files.cpp source/util/log_upload.cpp tests/log_upload/test.cpp
calendar_FLAGS := -std=gnu11 -Isource/core
calendar_SRC := source/core/calendar.c tests/calendar/test.c
support_SRC := source/util/date.cpp source/util/support.cpp tests/support/test.cpp
patches_SRC := source/util/paths.cpp source/util/patches.cpp tests/patches/test.cpp
number_SRC := tests/number/test.cpp
listeners_SRC := tests/listeners/test.cpp
error_hint_logic_SRC := source/action/error_hint_logic.cpp tests/error_hint_logic/test.cpp
write_queue_logic_SRC := source/action/write_queue_logic.cpp tests/write_queue_logic/test.cpp
write_queue_logic_LIBS := -pthread
sim_local_time_SRC := source/sim/sim_env.cpp tests/sim_local_time/test.cpp
sim_local_time_CSRC := source/sim/sim_local_time.c source/core/calendar.c
sim_local_time_CFLAGS := -std=gnu11 -Isource/core
shared_bell_logic_SRC   := source/action/shared_bell_logic.cpp source/util/date.cpp source/util/update.cpp tests/shared_bell_logic/test.cpp
shared_bell_logic_CSRC  := library/boreale-link/source/bl_kv.c library/boreale-link/source/bl_json.c library/boreale-link/source/bl_jread.c library/boreale-link/source/bl_app.c library/boreale-link/source/bl_notices.c library/boreale-link/source/bl_updates.c $(CALENDAR_C)
shared_bell_logic_CFLAGS := -std=gnu11 -Ilibrary/boreale-link/source -Isource/core
shared_look_SRC   := source/action/shared_look.cpp tests/shared_look/test.cpp
shared_look_CSRC  := library/boreale-link/source/bl_kv.c library/boreale-link/source/bl_prefs.c
shared_look_CFLAGS := -std=gnu11 -Ilibrary/boreale-link/source
boreale_apps_logic_SRC   := source/action/boreale_apps_logic.cpp source/action/shared_bell_logic.cpp source/util/date.cpp source/util/update.cpp tests/boreale_apps_logic/test.cpp
boreale_apps_logic_CSRC  := library/boreale-link/source/bl_kv.c library/boreale-link/source/bl_json.c library/boreale-link/source/bl_jread.c library/boreale-link/source/bl_app.c library/boreale-link/source/bl_notices.c library/boreale-link/source/bl_updates.c $(CALENDAR_C)
boreale_apps_logic_CFLAGS := -std=gnu11 -Ilibrary/boreale-link/source -Isource/core

# ---------------------------------------------------------------- the runner
TESTS := $(sort $(patsubst %_SRC,%,$(filter %_SRC,$(.VARIABLES))))
TEST_PROGRAMS := $(addprefix $(TESTOUT)/,$(TESTS))
TEST_OKS := $(addsuffix .ok,$(TEST_PROGRAMS))
# Every header: a suite is rebuilt when one changes (coarse, but a suite
# compiles in seconds and CI always starts cold).
TEST_HEADERS := $(wildcard tests/*.h tests/*.hpp tests/*/*.h tests/*/*.hpp source/*.h source/*.hpp source/*/*.h source/*/*.hpp)

$(TESTOUT):
	@mkdir -p $@

# make only compares dates: this file is rewritten when the compiler or the
# flags change (CC=clang, SAN=, COV=--coverage), so the suites are rebuilt.
TEST_FLAGS_FILE := $(TESTOUT)/flags
$(TEST_FLAGS_FILE): FORCE | $(TESTOUT)
	@echo "$(CC) $(CXX) $(CWARN)" | cmp -s - $@ || echo "$(CC) $(CXX) $(CWARN)" > $@
FORCE:

.SECONDEXPANSION:
test_flags  = $(or $($1_FLAGS),$(if $(filter %.cpp,$($1_SRC)),$(TEST_CXXFLAGS)))
test_csrc   = $(or $($1_CSRC),$(if $(filter source/util/date.cpp,$($1_SRC)),$(CALENDAR_C)))
test_cflags = $(if $($1_CSRC),$($1_CFLAGS),$(CALENDAR_CFLAGS))
test_cobj   = $(TESTOUT)/$1.$(basename $(notdir $2)).o
test_cobjs  = $(foreach c,$(call test_csrc,$1),$(call test_cobj,$1,$c))
$(TEST_PROGRAMS): $(TESTOUT)/%: $$($$*_SRC) $$(call test_csrc,$$*) $(TEST_HEADERS) $(TEST_FLAGS_FILE) | $(TESTOUT)
	$(foreach c,$(call test_csrc,$*),$(CC) $(call test_cflags,$*) $(CWARN) -c $c -o $(call test_cobj,$*,$c) &&) true
	$(if $(filter %.cpp,$($*_SRC)),$(CXX),$(CC)) $(call test_flags,$*) $(CWARN) $($*_SRC) $(call test_cobjs,$*) $($*_LIBS) -o $@

# Running it: the output goes to <name>.log and the terminal; <name>.ok only
# when it passed.
$(TEST_OKS): $(TESTOUT)/%.ok: $(TESTOUT)/%
	@rm -f $@
	@$< > $<.log 2>&1; r=$$?; cat $<.log; if [ $$r -eq 0 ]; then touch $@; else echo "$*: exit status $$r" >&2; exit $$r; fi

# All of them, in parallel, and every failing one reported, not the first.
test:
	@test -f $(JSON_HPP) || { echo "Aurore is missing (nlohmann/json for the tests): run git submodule update --init"; exit 1; }
	@rm -f $(TEST_OKS)
	@$(MAKE) --no-print-directory -k $(if $(findstring -j,$(MAKEFLAGS)),,-j$(JOBS)) -Otarget $(TEST_OKS)

check: test
	python3 tools/check_sources.py .
	python3 tools/check_resources.py .

clean:
	rm -rf $(BUILD)
