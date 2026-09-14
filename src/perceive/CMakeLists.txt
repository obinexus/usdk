add_library(usdk_perceive SHARED
    perceive.c
    ${PROJECT_SOURCE_DIR}/src/ffi/json_min.c
)
add_library(Usdk::perceive ALIAS usdk_perceive)
# No "lib" prefix: manifests/usdk-perceive.usdk-manifest.json's
# artifact_path is the bare name "usdk_perceive", resolved by usdk-ffi as
# "<manifest_dir>/usdk_perceive<platform-ext>" - CMake's MinGW default of
# prefixing SHARED libraries with "lib" would make the file it actually
# builds (libusdk_perceive.dll) not match that lookup.
set_target_properties(usdk_perceive PROPERTIES PREFIX "")

target_include_directories(usdk_perceive PRIVATE ${PROJECT_SOURCE_DIR}/src/ffi)
target_link_libraries(usdk_perceive PUBLIC Usdk::contracts)
target_compile_definitions(usdk_perceive PRIVATE USDK_BUILDING_SHARED)

install(TARGETS usdk_perceive
    EXPORT UsdkTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
