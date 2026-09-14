add_library(usdk_core SHARED
    core.c
    journal.c
)
add_library(Usdk::core ALIAS usdk_core)

target_include_directories(usdk_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(usdk_core PUBLIC Usdk::contracts Usdk::ffi)
target_compile_definitions(usdk_core PRIVATE USDK_BUILDING_SHARED)

install(TARGETS usdk_core
    EXPORT UsdkTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
