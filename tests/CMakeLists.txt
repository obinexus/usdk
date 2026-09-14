# --- Unit tests: pure logic, no subprocesses, no network -------------------

add_executable(test_sha256 unit/test_sha256.c)
target_link_libraries(test_sha256 PRIVATE Usdk::contracts)
add_test(NAME test_sha256 COMMAND $<TARGET_FILE:test_sha256>)

add_executable(test_candidate unit/test_candidate.c)
target_link_libraries(test_candidate PRIVATE Usdk::contracts)
add_test(NAME test_candidate COMMAND $<TARGET_FILE:test_candidate>)

add_executable(test_manifest unit/test_manifest.c ${PROJECT_SOURCE_DIR}/src/ffi/json_min.c ${PROJECT_SOURCE_DIR}/src/ffi/manifest.c)
target_include_directories(test_manifest PRIVATE ${PROJECT_SOURCE_DIR}/src/ffi)
target_link_libraries(test_manifest PRIVATE Usdk::contracts)
# manifest.c is compiled directly into this executable, not consumed
# through libusdk_ffi - USDK_STATIC makes USDK_EXPORT (usdk/platform.h)
# resolve to a plain declaration instead of dllimport, which is what a
# direct same-binary definition needs on Windows (otherwise the linker
# looks for an import-library thunk that was never generated for this
# target).
target_compile_definitions(test_manifest PRIVATE USDK_STATIC)
add_test(NAME test_manifest COMMAND $<TARGET_FILE:test_manifest>)

add_executable(test_core_consensus unit/test_core_consensus.c)
target_link_libraries(test_core_consensus PRIVATE Usdk::core Usdk::contracts)
add_test(NAME test_core_consensus COMMAND $<TARGET_FILE:test_core_consensus>)
set_tests_properties(test_core_consensus PROPERTIES TIMEOUT 30)

add_executable(test_perceive unit/test_perceive.c)
target_link_libraries(test_perceive PRIVATE Usdk::perceive Usdk::contracts)
add_test(NAME test_perceive COMMAND $<TARGET_FILE:test_perceive>)

add_executable(test_verify unit/test_verify.c)
target_link_libraries(test_verify PRIVATE Usdk::verify Usdk::contracts)
add_test(NAME test_verify COMMAND $<TARGET_FILE:test_verify>)

add_executable(test_deliberate unit/test_deliberate.c)
target_link_libraries(test_deliberate PRIVATE Usdk::deliberate Usdk::contracts)
target_compile_definitions(test_deliberate PRIVATE
    USDK_TEST_DRIVER_FIXTURE_PATH="$<TARGET_FILE:usdk_driver_fixture>"
)
add_dependencies(test_deliberate usdk_driver_fixture)
add_test(NAME test_deliberate COMMAND $<TARGET_FILE:test_deliberate>)

# --- Fixture: a deliberately ABI-incompatible module ------------------------

add_library(module_bad_abi MODULE fixtures/module_bad_abi.c)
set_target_properties(module_bad_abi PROPERTIES PREFIX "" C_VISIBILITY_PRESET hidden)
target_link_libraries(module_bad_abi PRIVATE Usdk::contracts)
target_compile_definitions(module_bad_abi PRIVATE USDK_BUILDING_SHARED)

# --- Integration tests: real dynamic loading --------------------------------

add_executable(test_ffi_abi_mismatch integration/test_ffi_abi_mismatch.c)
target_link_libraries(test_ffi_abi_mismatch PRIVATE Usdk::ffi Usdk::contracts)
target_compile_definitions(test_ffi_abi_mismatch PRIVATE
    USDK_TEST_BAD_ABI_MODULE_PATH="$<TARGET_FILE:module_bad_abi>"
)
add_dependencies(test_ffi_abi_mismatch module_bad_abi)
add_test(NAME test_ffi_abi_mismatch COMMAND $<TARGET_FILE:test_ffi_abi_mismatch>)

add_executable(test_ffi_load_resolved integration/test_ffi_load_resolved.c)
target_link_libraries(test_ffi_load_resolved PRIVATE Usdk::ffi Usdk::contracts)
target_compile_definitions(test_ffi_load_resolved PRIVATE
    USDK_TEST_MANIFEST_DIR="$<TARGET_FILE_DIR:usdk_cli>"
)
add_dependencies(test_ffi_load_resolved usdk_cli usdk_perceive usdk_deliberate usdk_verify usdk_driver_fixture)
add_test(NAME test_ffi_load_resolved COMMAND $<TARGET_FILE:test_ffi_load_resolved>)

# --- CLI / example smoke tests: exercise the documented commands end to end

add_test(NAME cli_doctor COMMAND $<TARGET_FILE:usdk_cli> doctor --json)
add_dependencies(usdk_cli usdk_perceive usdk_deliberate usdk_verify usdk_driver_fixture)

add_test(NAME cli_inspect_perceive COMMAND $<TARGET_FILE:usdk_cli> inspect usdk-perceive --json)

add_test(NAME cli_demo_trilateral_consensus
    COMMAND $<TARGET_FILE:usdk_cli> demo --scenario trilateral-consensus --json)
set_tests_properties(cli_demo_trilateral_consensus PROPERTIES TIMEOUT 30)

add_test(NAME example_perceive_standalone COMMAND $<TARGET_FILE:example_perceive_standalone>)
add_test(NAME example_verify_standalone COMMAND $<TARGET_FILE:example_verify_standalone>)
add_test(NAME example_deliberate_standalone COMMAND $<TARGET_FILE:example_deliberate_standalone>)
add_test(NAME example_trilateral_integration COMMAND $<TARGET_FILE:example_trilateral_integration>)
