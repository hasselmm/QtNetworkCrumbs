qnc_add_library(
    QncUpnp STATIC
    upnpresolver.cpp
    upnpresolver.h
    upnptreemodel.cpp
    upnptreemodel.h
)

target_link_libraries(QncUpnp PUBLIC QncSsdp QncXml)

if (NOT IOS) # FIXME Figure out code signing on Github
    qnc_add_executable(upnpresolverdemo TYPE tool upnpresolverdemo.cpp)
    target_link_libraries(upnpresolverdemo PRIVATE QncUpnp)
endif()
