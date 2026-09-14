add_executable(example_perceive_standalone main.c)
target_link_libraries(example_perceive_standalone PRIVATE Usdk::perceive Usdk::contracts)
