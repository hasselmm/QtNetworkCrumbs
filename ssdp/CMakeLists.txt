qnc_add_library(
    QncSsdp STATIC
    ssdpresolver.cpp
    ssdpresolver.h
)

target_link_libraries(QncSsdp PUBLIC QncHttp)

if (NOT IOS) # FIXME Figure out code signing on Github
    qnc_add_executable(ssdpresolverdemo TYPE tool ssdpresolverdemo.cpp)
    target_link_libraries(ssdpresolverdemo PRIVATE QncSsdp)
endif()
