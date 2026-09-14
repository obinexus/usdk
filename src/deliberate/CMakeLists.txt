add_library(usdk_deliberate SHARED
    deliberate.c
    ${PROJECT_SOURCE_DIR}/src/ffi/json_min.c
)
add_library(Usdk::deliberate ALIAS usdk_deliberate)
# See the identical comment in src/perceive/CMakeLists.txt - the manifest
# artifact_path is a bare name, and CMake's MinGW "lib" prefix default
# would make the built file not match it.
set_target_properties(usdk_deliberate PROPERTIES PREFIX "")

target_include_directories(usdk_deliberate PRIVATE ${PROJECT_SOURCE_DIR}/src/ffi)
# usdk-deliberate is the one constituent that depends on usdk-ffi (to
# load its configured driver at runtime) - see docs/PACKAGES.md's
# dependency table. It does not link usdk-perceive, usdk-verify, or
# usdk-core.
target_link_libraries(usdk_deliberate PUBLIC Usdk::contracts Usdk::ffi)
target_compile_definitions(usdk_deliberate PRIVATE USDK_BUILDING_SHARED)

install(TARGETS usdk_deliberate
    EXPORT UsdkTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
