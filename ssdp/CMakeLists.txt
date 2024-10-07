qnc_add_library(
    QncSsdp STATIC
    ALIAS Qnc::Ssdp

    ssdpresolver.cpp
    ssdpresolver.h
)

target_link_libraries(QncSsdp PUBLIC Qnc::Http)

if (NOT IOS) # FIXME Figure out code signing on Github
    qnc_add_executable(SSDPResolverDemo TYPE tool ssdpresolverdemo.cpp)
    target_link_libraries(SSDPResolverDemo PRIVATE Qnc::Ssdp)
endif()
