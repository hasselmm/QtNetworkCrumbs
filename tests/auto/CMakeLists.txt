if (MINGW)
    get_filename_component(CXX_RUNTIME_BINDIR ${CMAKE_CXX_COMPILER} DIRECTORY)
    configure_file(test-runner.cmd.in test-runner.cmd NEWLINE_STYLE WIN32)
    set(TEST_RUNNER "${CMAKE_CURRENT_BINARY_DIR}/test-runner.cmd")
else()
    unset(TEST_RUNNER)
endif()

qnc_add_library(
    QncTestSuport STATIC
    ALIAS Qnc::TestSuport

    qnctestsupport.cpp
    qnctestsupport.h
)

target_link_libraries(QncTestSuport PUBLIC Qt::Test)

add_testcase(tst_coremodels.cpp   LIBRARIES Qnc::Core)
add_testcase(tst_coreparse.cpp    LIBRARIES Qnc::Core Qnc::TestSuport)
add_testcase(tst_httpparser.cpp   LIBRARIES Qnc::Http)
add_testcase(tst_mdnsmessages.cpp LIBRARIES Qnc::Mdns)
add_testcase(tst_mdnsresolver.cpp LIBRARIES Qnc::Mdns)
add_testcase(tst_ssdpresolver.cpp LIBRARIES Qnc::Ssdp)
add_testcase(tst_xmlparser.cpp    LIBRARIES Qnc::Xml Qnc::TestSuport)
