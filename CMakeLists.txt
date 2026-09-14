cmake_minimum_required(VERSION 3.20)

project(usdk
    VERSION 0.1.0
    DESCRIPTION "USDK - trilateral SDK for agentic language-model applications"
    LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Every shared library, executable, and dynamically-loaded driver module
# lands in one directory, matching Windows's DLL search rules
# (LoadLibraryExW with LOAD_LIBRARY_SEARCH_DEFAULT_DIRS looks only in the
# loading module's own directory, the application directory, and system
# directories - never PATH or CWD - so usdk_core.dll and every driver
# module must sit next to whatever .exe or .dll loads them).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(USDK_ENABLE_TESTS "Build the test suite" ON)
option(USDK_ENABLE_SANITIZERS "Build with -fsanitize=address,undefined (GCC/Clang debug builds)" OFF)
option(USDK_EMBED_SOURCE_DIR_FALLBACK
    "Embed this build's source directory as a dev-convenience fallback for locating uninstalled resources (never for packaging)"
    ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GNUInstallDirs)

if(USDK_ENABLE_SANITIZERS)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(WARNING "USDK_ENABLE_SANITIZERS requested but compiler ${CMAKE_C_COMPILER_ID} is not GCC/Clang; ignoring")
    endif()
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpointer-arith)
endif()

add_subdirectory(src/contracts)
add_subdirectory(src/core)
add_subdirectory(src/ffi)
add_subdirectory(src/perceive)
add_subdirectory(src/deliberate)
add_subdirectory(src/verify)
add_subdirectory(src/driver_fixture)
add_subdirectory(src/cli)

if(USDK_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

add_subdirectory(examples)

# The manifest JSON files are copied into CMAKE_RUNTIME_OUTPUT_DIRECTORY
# by src/cli/CMakeLists.txt (attached to usdk_cli, the target that
# actually needs them at runtime - add_custom_command(TARGET ... POST_BUILD)
# must be called from the directory scope that created the target).
file(GLOB USDK_MANIFEST_FILES "${PROJECT_SOURCE_DIR}/manifests/*.usdk-manifest.json")
install(FILES ${USDK_MANIFEST_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR})

# --- Package install / export ------------------------------------------------

install(FILES examples/config/validate-config.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/usdk/examples)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/UsdkConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/UsdkConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/UsdkConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/usdk")

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/UsdkConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/UsdkConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/usdk")

install(EXPORT UsdkTargets
    FILE UsdkTargets.cmake
    NAMESPACE Usdk::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/usdk")

install(FILES LICENSE README.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/usdk)
install(FILES
    docs/ARCHITECTURE.md
    docs/PACKAGES.md
    docs/CONSENSUS_PROTOCOL.md
    docs/ABI.md
    docs/GETTING_STARTED.md
    docs/VALIDATION.md
    docs/IMPLEMENTATION_STATUS.md
    docs/RESEARCH_REVIEW.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/usdk)

message(STATUS "usdk ${PROJECT_VERSION} : tests=${USDK_ENABLE_TESTS} sanitizers=${USDK_ENABLE_SANITIZERS}")
