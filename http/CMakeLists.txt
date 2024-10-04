qnc_add_library(
    QncHttp STATIC
    ALIAS Qnc::Http

    httpparser.cpp
    httpparser.h
)

target_link_libraries(QncHttp PUBLIC Qnc::Core)

if (NOT IOS) # FIXME Figure out code signing on Github
    qnc_add_executable(HttpCompressingServer TYPE tool compressingserver.cpp)
    target_link_libraries(HttpCompressingServer PUBLIC Qt::Network QtNetworkCrumbs::Zlib)
endif()
