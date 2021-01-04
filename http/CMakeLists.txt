add_executable(HttpCompressingServer compressingserver.cpp)
target_link_libraries(HttpCompressingServer PUBLIC Qt5::Network)

if (TARGET ZLIB::ZLIB)
    target_link_libraries(HttpCompressingServer PUBLIC ZLIB::ZLIB)
elseif(TARGET Qt5::Zlib)
    target_link_libraries(HttpCompressingServer PUBLIC Qt5::Zlib)
endif()
