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
# -Itests: check.h, the CHECK() every suite uses; -Itests/support: the app_id.h
# of the app the tests stand in for. _GLIBCXX_ASSERTIONS: libstdc++ checks
# what the sanitizers cannot see.
CWARN   := -Wall -Wextra -Wshadow -Werror -D_GLIBCXX_ASSERTIONS $(SAN) $(COV) -Itests -Itests/support
JSON_HPP := library/aurore/library/include/aurore/extern/nlohmann/json.hpp

.PHONY: clean test check FORCE

# Aurore's headers (nlohmann/json) as system headers: their warnings are not
# the kit's.
TEST_CXXFLAGS   := -std=c++23 -Isource -Ilibrary/boreale-link/source -isystem library/aurore/library/include
CALENDAR_C      := source/core/calendar.c
CALENDAR_CFLAGS := -std=gnu11 -Isource/core

listeners_SRC := tests/listeners/test.cpp

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

clean:
	rm -rf $(BUILD)
