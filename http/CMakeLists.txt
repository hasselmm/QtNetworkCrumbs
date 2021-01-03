add_executable(HttpCompressingServer compressingserver.cpp)
target_link_libraries(HttpCompressingServer PUBLIC Qt5::Network Qt5::Zlib)
