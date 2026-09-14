add_library(usdk_verify SHARED
    verify.c
    ${PROJECT_SOURCE_DIR}/src/ffi/json_min.c
)
add_library(Usdk::verify ALIAS usdk_verify)
# See the identical comment in src/perceive/CMakeLists.txt - the manifest
# artifact_path is a bare name, and CMake's MinGW "lib" prefix default
# would make the built file not match it.
set_target_properties(usdk_verify PROPERTIES PREFIX "")

target_include_directories(usdk_verify PRIVATE ${PROJECT_SOURCE_DIR}/src/ffi)
target_link_libraries(usdk_verify PUBLIC Usdk::contracts)
target_compile_definitions(usdk_verify PRIVATE USDK_BUILDING_SHARED)

install(TARGETS usdk_verify
    EXPORT UsdkTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
