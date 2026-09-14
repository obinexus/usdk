# Usage: cmake -DUSDK_CHECK_DIR=<build-dir> -P cmake/CheckBuildEnv.cmake
#
# Invoked by the root Makefile's `configure` target, before it touches the
# build directory. If USDK_CHECK_DIR already holds a configured cache,
# this warns (does not fail) when that cache's compiler looks like an
# MSYS2/MinGW toolchain (ucrt64/mingw64/clang64/msys64 in its path) but
# the *current* shell does not have that MSYS2 environment active
# (MSYSTEM is unset).
#
# This is not hypothetical, and it has two real, distinct forms, both
# reproduced directly while building this project:
#
#  1. MSYSTEM unset entirely - e.g. a UCRT64-configured cache reused
#     from plain PowerShell/cmd that merely has C:\msys64\ucrt64\bin on
#     PATH, but was never launched as an MSYS2 shell at all.
#  2. MSYSTEM set, but to the WRONG subsystem - e.g. a cache configured
#     with the UCRT64 compiler, reused from a real MSYS2 shell that is
#     itself MINGW64 (or any subsystem other than the one the cache was
#     configured with). Mixing subsystems this way produces objects that
#     don't link even when each individual compiler invocation appears
#     to succeed.
#
# In both cases CMake finds the cached absolute compiler path fine and
# starts configuring, but the actual compile step then fails ("is not
# able to compile a simple test program") with no useful diagnostic
# pointing at the real cause: the compiler driver (cc.exe) can often
# still run standalone, but the real compiler back end it execs needs
# its own subsystem's runtime DLLs found via PATH, which are only
# present inside a shell actually activated for that exact subsystem
# (see the ucrt64.exe / "MSYS2 UCRT64" launcher, which sets this up -
# just having ucrt64/bin on PATH, or being in a differently-flavored
# MSYS2 shell, is not the same thing).

if(NOT DEFINED USDK_CHECK_DIR)
  message(FATAL_ERROR "CheckBuildEnv.cmake: USDK_CHECK_DIR was not set")
endif()

set(_cache "${USDK_CHECK_DIR}/CMakeCache.txt")
if(NOT EXISTS "${_cache}")
  return() # nothing configured yet in this directory - nothing to check
endif()

file(STRINGS "${_cache}" _compiler_line REGEX "^CMAKE_C_COMPILER:[A-Za-z]+=")
if(NOT _compiler_line)
  return()
endif()

# The cache entry's type (normally FILEPATH) can end up as STRING instead
# after a failed compiler check writes it back differently - match either
# rather than hardcoding one, and strip whichever type was actually there.
string(REGEX REPLACE "^CMAKE_C_COMPILER:[A-Za-z]+=" "" _compiler "${_compiler_line}")
string(TOLOWER "${_compiler}" _compiler_lower)

# Capture WHICH subsystem the cached compiler belongs to (not just
# whether it looks like MSYS2 at all), so a shell activated for a
# different subsystem can be distinguished from no MSYS2 environment at
# all - see the two-form explanation above.
#
# Deliberately does NOT include "msys64" as a marker: that is the common
# MSYS2 *install root* directory name (as in C:\msys64\), not a subsystem
# - it is a substring of every single path under a standard install
# (including "C:/msys64/ucrt64/bin/cc.exe"), so including it here
# previously caused it to falsely override a correct "ucrt64" match,
# caught by actually running this check against this project's own
# UCRT64-configured cache from a MINGW64 shell and seeing it misreport
# the subsystem as "msys64" instead of "ucrt64". The base MSYS subsystem
# itself is named "MSYS" (MSYSTEM=MSYS) with compilers, if any, under
# .../usr/bin - not matched here, since this project has no need to
# special-case it.
set(_matched_marker "")
foreach(_marker ucrt64 mingw64 mingw32 clang64 clangarm64)
  if(_compiler_lower MATCHES "${_marker}")
    set(_matched_marker "${_marker}")
  endif()
endforeach()

if(NOT _matched_marker STREQUAL "")
  string(TOLOWER "$ENV{MSYSTEM}" _current_msystem_lower)
  if(_current_msystem_lower STREQUAL "")
    message(WARNING
      "The build directory '${USDK_CHECK_DIR}' was previously configured "
      "with an MSYS2/MinGW compiler:\n"
      "    ${_compiler}\n"
      "but this shell does not have any MSYS2 environment active "
      "(the MSYSTEM environment variable is unset) - even if that "
      "compiler's directory happens to be on PATH here.\n"
      "The compiler driver can appear to work in this shell (e.g. running "
      "it with --version succeeds) while real compilation still fails, "
      "because the compiler's back end cannot find its own runtime DLLs "
      "outside that environment - the failure shows up mid-build with "
      "little useful output, not here.\n"
      "Fix: run make from the matching MSYS2 shell (e.g. C:\\msys64\\ucrt64.exe, "
      "or a UCRT64 'MSYS2 UCRT64' shortcut), or build into a separate "
      "directory for this shell's own toolchain, e.g.:\n"
      "    make BUILD_DIR=build-other")
  elseif(NOT _current_msystem_lower STREQUAL _matched_marker)
    message(WARNING
      "The build directory '${USDK_CHECK_DIR}' was previously configured "
      "with a compiler from the '${_matched_marker}' MSYS2 subsystem:\n"
      "    ${_compiler}\n"
      "but this shell is activated for a DIFFERENT subsystem "
      "(MSYSTEM=$ENV{MSYSTEM}). Mixing subsystems this way produces "
      "objects that don't link, even when individual compiler "
      "invocations appear to succeed here.\n"
      "Fix: run make from a '${_matched_marker}' shell instead (e.g. "
      "C:\\msys64\\${_matched_marker}.exe), or build into a separate "
      "directory for this shell's own subsystem, e.g.:\n"
      "    make BUILD_DIR=build-$ENV{MSYSTEM}")
  endif()
endif()
