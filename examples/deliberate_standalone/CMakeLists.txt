add_executable(example_deliberate_standalone main.c)
target_link_libraries(example_deliberate_standalone PRIVATE Usdk::deliberate Usdk::contracts)
# Not linked (loaded dynamically at runtime via usdk-ffi - see main.c),
# but must exist in the shared output directory before this example runs.
add_dependencies(example_deliberate_standalone usdk_driver_fixture)
