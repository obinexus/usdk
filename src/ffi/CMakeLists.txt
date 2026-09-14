add_library(usdk_ffi SHARED
    ffi.c
    manifest.c
    json_min.c
    platform_load_win.c
    platform_load_unix.c
)
add_library(Usdk::ffi ALIAS usdk_ffi)

target_include_directories(usdk_ffi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(usdk_ffi PUBLIC Usdk::contracts)
target_compile_definitions(usdk_ffi PRIVATE USDK_BUILDING_SHARED)

if(UNIX)
    target_link_libraries(usdk_ffi PRIVATE dl)
endif()

install(TARGETS usdk_ffi
    EXPORT UsdkTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
