add_executable(usdk_cli
    main.c
    ${PROJECT_SOURCE_DIR}/src/ffi/json_min.c
)
set_target_properties(usdk_cli PROPERTIES OUTPUT_NAME "usdk")
target_include_directories(usdk_cli PRIVATE ${PROJECT_SOURCE_DIR}/src/ffi)
# The CLI's `demo` command links the three constituents directly (their
# full public API, including operations like usdk_perceive_observe that
# have no generic-ABI vtable equivalent - see the comment on run_round()
# in main.c); `doctor`/`inspect`/`validate` separately exercise the
# dynamic-loading path through Usdk::ffi. This is permitted - the
# architecture forbids the three constituents from linking each other
# (docs/PACKAGES.md), not a support package like usdk-cli linking them.
target_link_libraries(usdk_cli PRIVATE
    Usdk::core Usdk::ffi Usdk::contracts
    Usdk::perceive Usdk::deliberate Usdk::verify
)

install(TARGETS usdk_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# usdk_cli defaults its manifest_dir to its own directory (own_exe_dir()
# in main.c) - see docs/GETTING_STARTED.md - so the manifest JSON files
# need to land next to the built binary in the shared output directory,
# not stay only in the source tree. Attached here (POST_BUILD on
# usdk_cli), not the root CMakeLists.txt, because add_custom_command(TARGET
# ... POST_BUILD) must be called from the directory scope that created
# the target.
file(GLOB USDK_MANIFEST_FILES "${PROJECT_SOURCE_DIR}/manifests/*.usdk-manifest.json")
foreach(_m ${USDK_MANIFEST_FILES})
    add_custom_command(TARGET usdk_cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${_m}" "$<TARGET_FILE_DIR:usdk_cli>/"
    )
endforeach()
