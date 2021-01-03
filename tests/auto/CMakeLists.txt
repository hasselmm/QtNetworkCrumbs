add_executable(tst_mdnsmessages tst_mdnsmessages.cpp)
target_link_libraries(tst_mdnsmessages PRIVATE MDNSResolver Qt5::Test)

