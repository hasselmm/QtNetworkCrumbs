add_executable(MDNSResolverDemo servicediscovery.cpp)
target_link_libraries(MDNSResolverDemo PRIVATE MDNSResolver)

if (WIN32)
    add_executable(WinDNSTest windnstest.cpp)
    target_link_libraries(WinDNSTest -ldnsapi)
endif()

