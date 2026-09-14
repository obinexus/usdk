# A loadable module only - never linked directly by anything at build
# time (usdk-deliberate loads it purely at runtime via usdk-ffi), so this
# is a MODULE library, not SHARED - matching the distinction documented
# in docs/PACKAGES.md.
add_library(usdk_driver_fixture MODULE
    driver_fixture.c
)
set_target_properties(usdk_driver_fixture PROPERTIES
    PREFIX ""
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
target_link_libraries(usdk_driver_fixture PRIVATE Usdk::contracts)
target_compile_definitions(usdk_driver_fixture PRIVATE USDK_BUILDING_SHARED)

# Installed to the SAME directory as every role library and the CLI
# (${CMAKE_INSTALL_BINDIR}), not a separate drivers/ subdirectory -
# manifests declare `artifact_path` as a bare module name resolved
# relative to `manifest_dir`, and the CLI always passes its own
# directory as `manifest_dir` (see src/cli/main.c and
# docs/GETTING_STARTED.md), so every loadable module living in one place
# is what keeps that resolution correct in both the build tree (where
# CMAKE_LIBRARY_OUTPUT_DIRECTORY already puts every target in one bin/)
# and the installed tree, without two different path shapes to keep in
# sync - the exact class of bug documented as found and fixed in a
# sibling OBINexus project's packaging work (not this task's research
# archives - an unrelated prior engineering lesson applied here
# proactively).
install(TARGETS usdk_driver_fixture
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
