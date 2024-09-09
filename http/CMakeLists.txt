if (NOT IOS) # FIXME Figure out code signing on Github
    qnc_add_executable(HttpCompressingServer TYPE tool compressingserver.cpp)
    target_link_libraries(HttpCompressingServer PUBLIC Qt::Network QtNetworkCrumbs::Zlib)
endif()
