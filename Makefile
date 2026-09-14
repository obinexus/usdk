# USDK - root Makefile
#
# A thin, portable wrapper around the real build system, which is CMake
# (see CMakeLists.txt and cmake/). This file exists only to make the
# documented CMake workflow reachable by typing plain `make` - it does
# not duplicate the CMake target graph (see docs/PACKAGES.md for what
# actually builds what).
#
# GNU Make on Windows picks its recipe shell by searching for sh.exe; if
# none is found it falls back to cmd.exe - which shell that is has
# nothing to do with which shell *launched* `make`. Every recipe below is
# written to mean the same thing under both: plain `$(CMAKE)`/`$(CTEST)`
# invocations with no shell operators beyond the one redirect (`2>&1`)
# both dialects agree on, and `cmake -E` for every directory/file
# operation instead of shell builtins like `mkdir -p`/`rm -rf`/`if exist`.

# --------------------------------------------------------------------------
# Overridable variables (e.g. `make BUILD_TYPE=Release JOBS=8`)
# --------------------------------------------------------------------------

CMAKE          ?= cmake
CTEST          ?= ctest
BUILD_DIR      ?= build
BUILD_TYPE     ?= Debug
GENERATOR      ?=
INSTALL_PREFIX ?= $(CURDIR)/install
JOBS           ?=

BUILD_DIR_RELEASE ?= $(BUILD_DIR)-release

# --------------------------------------------------------------------------
# Derived flags
# --------------------------------------------------------------------------

ifneq ($(strip $(GENERATOR)),)
CONFIGURE_GENERATOR_FLAG := -G "$(GENERATOR)"
else
CONFIGURE_GENERATOR_FLAG :=
endif

ifneq ($(strip $(JOBS)),)
BUILD_PARALLEL_FLAG := --parallel $(JOBS)
TEST_PARALLEL_FLAG   := -j $(JOBS)
else
BUILD_PARALLEL_FLAG :=
TEST_PARALLEL_FLAG   :=
endif

# --------------------------------------------------------------------------
# Tool checks - skipped for a bare `make help`, since that should work
# even before CMake is installed.
# --------------------------------------------------------------------------

ifeq ($(MAKECMDGOALS),help)
NEEDS_CMAKE_CHECK :=
else
NEEDS_CMAKE_CHECK := 1
endif

ifdef NEEDS_CMAKE_CHECK
CMAKE_CHECK_OUTPUT := $(shell $(CMAKE) --version 2>&1)
ifeq ($(findstring cmake version,$(CMAKE_CHECK_OUTPUT)),)
$(error CMake was not found (tried to run: '$(CMAKE)'). Install CMake 3.20+ \
and ensure it is on PATH, or pass CMAKE=/full/path/to/cmake. \
See docs/GETTING_STARTED.md for MSYS2 UCRT64 setup. \
Raw output was: $(CMAKE_CHECK_OUTPUT))
endif
endif

ifneq ($(filter test,$(MAKECMDGOALS)),)
CTEST_CHECK_OUTPUT := $(shell $(CTEST) --version 2>&1)
ifeq ($(findstring ctest version,$(CTEST_CHECK_OUTPUT)),)
$(error CTest was not found (tried to run: '$(CTEST)'). It ships with \
CMake - if cmake works but ctest does not, add CMake's bin directory to \
PATH, or pass CTEST=/full/path/to/ctest. Raw output was: $(CTEST_CHECK_OUTPUT))
endif
endif

.PHONY: all configure build debug release test install clean rebuild help

all: build

.DEFAULT_GOAL := all

# --------------------------------------------------------------------------
# configure - generate (or refresh) the CMake build system in $(BUILD_DIR).
# Safe to re-run: an existing cache is reused, never deleted.
# --------------------------------------------------------------------------

configure:
	$(CMAKE) -DUSDK_CHECK_DIR="$(BUILD_DIR)" -P cmake/CheckBuildEnv.cmake
	$(CMAKE) -S . -B "$(BUILD_DIR)" $(CONFIGURE_GENERATOR_FLAG) -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)"

# --------------------------------------------------------------------------
# build - build every target CMakeLists.txt defines via CMake's own
# default `all` target - not an invented, separately-maintained list of
# target names. --config is unconditional: ignored for a single-
# configuration generator (Ninja, Makefiles), required for a multi-
# configuration one (Visual Studio, Xcode).
# --------------------------------------------------------------------------

build: configure
	$(CMAKE) --build "$(BUILD_DIR)" --config "$(BUILD_TYPE)" $(BUILD_PARALLEL_FLAG)

# --------------------------------------------------------------------------
# debug / release
# --------------------------------------------------------------------------

debug:
	$(MAKE) BUILD_TYPE=Debug BUILD_DIR="$(BUILD_DIR)" build

release:
	$(MAKE) BUILD_TYPE=Release BUILD_DIR="$(BUILD_DIR_RELEASE)" build

# --------------------------------------------------------------------------
# test - build first, then run the suite with failure output on.
# --------------------------------------------------------------------------

test: build
	$(CTEST) --test-dir "$(BUILD_DIR)" -C "$(BUILD_TYPE)" --output-on-failure $(TEST_PARALLEL_FLAG)

# --------------------------------------------------------------------------
# install - build first, then install to an overridable *local* prefix
# (default: ./install under this repository, not a system directory).
# --------------------------------------------------------------------------

install: build
	$(CMAKE) -E make_directory "$(INSTALL_PREFIX)"
	$(CMAKE) --install "$(BUILD_DIR)" --config "$(BUILD_TYPE)" --prefix "$(INSTALL_PREFIX)"

# --------------------------------------------------------------------------
# clean - remove compiled outputs via CMake's own generator-agnostic
# `clean` target, without touching CMakeCache.txt or the generated build
# files, so the next build does not need to reconfigure from scratch.
# --------------------------------------------------------------------------

clean:
ifneq ($(wildcard $(BUILD_DIR)/CMakeCache.txt),)
	$(CMAKE) --build "$(BUILD_DIR)" --target clean
else
	$(CMAKE) -E echo "Nothing to clean - $(BUILD_DIR) is not configured."
endif

# --------------------------------------------------------------------------
# rebuild - clean then build, guaranteed sequential (two recipe lines of
# one target, not a `clean build` prerequisite list - see the identical
# reasoning in the OBINexus Obicall project's Makefile, which this one
# follows).
# --------------------------------------------------------------------------

rebuild:
	$(MAKE) clean
	$(MAKE) build

# --------------------------------------------------------------------------
# help
# --------------------------------------------------------------------------

help:
	@$(CMAKE) -E echo "USDK - root Makefile"
	@$(CMAKE) -E echo ""
	@$(CMAKE) -E echo "Targets:"
	@$(CMAKE) -E echo "  make            configure and build (default target)"
	@$(CMAKE) -E echo "  make configure  generate/refresh the CMake build system only"
	@$(CMAKE) -E echo "  make build      build every project target (implies configure)"
	@$(CMAKE) -E echo "  make debug      build the Debug configuration, into BUILD_DIR"
	@$(CMAKE) -E echo "  make release    build the Release configuration, into BUILD_DIR_RELEASE"
	@$(CMAKE) -E echo "  make test       build, then run ctest with failure output shown"
	@$(CMAKE) -E echo "  make install    build, then install under INSTALL_PREFIX"
	@$(CMAKE) -E echo "  make clean      remove compiled outputs; keeps the CMake cache"
	@$(CMAKE) -E echo "  make rebuild    clean, then build (always sequential, never -j together)"
	@$(CMAKE) -E echo "  make help       this message"
	@$(CMAKE) -E echo ""
	@$(CMAKE) -E echo "Variables (default shown, override as VAR=value):"
	@$(CMAKE) -E echo "  CMAKE=cmake"
	@$(CMAKE) -E echo "  CTEST=ctest"
	@$(CMAKE) -E echo "  BUILD_DIR=build"
	@$(CMAKE) -E echo "  BUILD_TYPE=Debug"
	@$(CMAKE) -E echo "  GENERATOR=            (empty: let CMake pick - see docs/GETTING_STARTED.md)"
	@$(CMAKE) -E echo "  INSTALL_PREFIX=<repo>/install"
	@$(CMAKE) -E echo "  JOBS=                 (empty: let the build tool pick its own default)"
	@$(CMAKE) -E echo "  BUILD_DIR_RELEASE=BUILD_DIR-release   (used only by 'make release')"
	@$(CMAKE) -E echo ""
	@$(CMAKE) -E echo "Examples:"
	@$(CMAKE) -E echo "  make JOBS=8"
	@$(CMAKE) -E echo "  make BUILD_DIR=build-ucrt64 GENERATOR=Ninja test"
	@$(CMAKE) -E echo "  make INSTALL_PREFIX=C:/opt/usdk install"
	@$(CMAKE) -E echo "  make release"
	@$(CMAKE) -E echo ""
	@$(CMAKE) -E echo "See docs/GETTING_STARTED.md for MSYS2 UCRT64 prerequisites."
