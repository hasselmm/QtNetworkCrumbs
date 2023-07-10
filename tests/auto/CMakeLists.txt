if (MINGW)
    get_filename_component(CXX_RUNTIME_BINDIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    configure_file(test-runner.cmd.in test-runner.cmd NEWLINE_STYLE WIN32)
    set(TEST_RUNNER ${CMAKE_CURRENT_BINARY_DIR}/test-runner.cmd)
else()
    unset(TEST_RUNNER)
endif()

message(STATUS "TEST_RUNNER: ${TEST_RUNNER}")

function(add_testcase NAME)
    add_executable(${NAME} ${NAME}.cpp)
    target_link_libraries(${NAME} PRIVATE MDNSResolver Qt5::Test)

    set_target_properties(${NAME} PROPERTIES
        MACOSX_BUNDLE "de.taschenorakel.qnc.${NAME}"
        XCODE_PRODUCT_TYPE "com.apple.product-type.bundle.unit-test")

    add_test(NAME ${NAME} COMMAND ${TEST_RUNNER} $<TARGET_FILE:${NAME}>)
endfunction()

add_testcase(tst_mdnsmessages)
add_testcase(tst_mdnsresolver)
