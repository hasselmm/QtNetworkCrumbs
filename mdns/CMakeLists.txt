qnc_add_library(
    QncMdns STATIC
    ALIAS Qnc::Mdns

    mdnsmessage.cpp
    mdnsmessage.h
    mdnsresolver.cpp
    mdnsresolver.h
    mdnsurlfinder.cpp
    mdnsurlfinder.h
)

target_link_libraries(QncMdns PUBLIC Qnc::Core)


if (NOT IOS) # FIXME Figure out code signing on Github
    qnc_add_executable(MDNSResolverDemo TYPE tool mdnsresolverdemo.cpp)
    target_link_libraries(MDNSResolverDemo PRIVATE Qnc::Mdns)
endif()

#if (WIN32)
#    qnc_add_executable(WinDNSTest TYPE tool windnstest.cpp)
#    target_link_libraries(WinDNSTest dnsapi.lib)
#endif()
