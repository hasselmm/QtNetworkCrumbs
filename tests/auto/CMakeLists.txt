if (MINGW)
    get_filename_component(CXX_RUNTIME_BINDIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    configure_file(test-runner.cmd.in test-runner.cmd NEWLINE_STYLE WIN32)
    set(TEST_RUNNER "${CMAKE_CURRENT_BINARY_DIR}/test-runner.cmd")
else()
    unset(TEST_RUNNER)
endif()

function(add_testcase SOURCE_FILENAME) # [SOURCES...]
    cmake_parse_arguments(TESTCASE "" "NAME" "LIBRARIES;SOURCES" ${ARGN})

    if (NOT TESTCASE_NAME)
        cmake_path(GET SOURCE_FILENAME STEM TESTCASE_NAME)
    endif()

    list(PREPEND TESTCASE_SOURCES "${SOURCE_FILENAME}")
    list(APPEND  TESTCASE_SOURCES ${TESTCASE_UNPARSED_ARGUMENTS})

    qnc_add_executable("${TESTCASE_NAME}" TYPE "unit-test" ${TESTCASE_SOURCES})
    target_link_libraries("${TESTCASE_NAME}" PRIVATE Qt::Test ${TESTCASE_LIBRARIES})
    add_test(NAME "${TESTCASE_NAME}" COMMAND ${TEST_RUNNER} "$<TARGET_FILE:${TESTCASE_NAME}>")
endfunction()

qnc_add_library(
    QncTestSuport STATIC
    qnctestsupport.cpp
    qnctestsupport.h
)

target_link_libraries(QncTestSuport PUBLIC Qt::Test)

add_testcase(tst_coremodels.cpp   LIBRARIES QncCore)
add_testcase(tst_coreparse.cpp    LIBRARIES QncCore QncTestSuport)
add_testcase(tst_httpparser.cpp   LIBRARIES QncHttp)
add_testcase(tst_mdnsmessages.cpp LIBRARIES QncMdns)
add_testcase(tst_mdnsresolver.cpp LIBRARIES QncMdns)
add_testcase(tst_ssdpresolver.cpp LIBRARIES QncSsdp)
add_testcase(tst_xmlparser.cpp    LIBRARIES QncXml QncTestSuport)
