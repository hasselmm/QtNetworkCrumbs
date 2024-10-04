if (MINGW)
    get_filename_component(CXX_RUNTIME_BINDIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    configure_file(test-runner.cmd.in test-runner.cmd NEWLINE_STYLE WIN32)
    set(TEST_RUNNER "${CMAKE_CURRENT_BINARY_DIR}/test-runner.cmd")
else()
    unset(TEST_RUNNER)
endif()

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
