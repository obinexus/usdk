add_library(usdk_contracts SHARED
    status.c
    sha256.c
    wire.c
    candidate.c
    util.c
)
add_library(Usdk::contracts ALIAS usdk_contracts)

target_include_directories(usdk_contracts PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
# USDK_EXPORT (include/usdk/platform.h) switches between dllexport and
# dllimport based on this definition, private to the target that is
# actually building the DLL - every *consumer* of usdk_contracts sees
# dllimport, which is what makes the shared-library build work on
# Windows without a separate generated export header.
target_compile_definitions(usdk_contracts PRIVATE USDK_BUILDING_SHARED)

install(TARGETS usdk_contracts
    EXPORT UsdkTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/usdk DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
