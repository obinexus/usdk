add_executable(example_verify_standalone main.c)
target_link_libraries(example_verify_standalone PRIVATE Usdk::verify Usdk::contracts)
