add_library(MDNSResolver SHARED)
target_include_directories(MDNSResolver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(MDNSResolver PRIVATE mdnsmessage.cpp mdnsmessage.h)
target_link_libraries(MDNSResolver PUBLIC Qt5::Network)

add_executable(MDNSResolverDemo servicediscovery.cpp)
target_link_libraries(MDNSResolverDemo PRIVATE MDNSResolver)

if (WIN32)
    add_executable(WinDNSTest windnstest.cpp)
    target_compile_definitions(WinDNSTest PRIVATE -DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP)
    target_link_libraries(WinDNSTest dnsapi.lib)
endif()
