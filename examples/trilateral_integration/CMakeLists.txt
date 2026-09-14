add_executable(example_trilateral_integration main.c)
target_link_libraries(example_trilateral_integration PRIVATE
    Usdk::perceive Usdk::deliberate Usdk::verify Usdk::core Usdk::contracts
)
add_dependencies(example_trilateral_integration usdk_driver_fixture)
