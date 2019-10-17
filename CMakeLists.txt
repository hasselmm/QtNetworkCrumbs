project(QtNetworkCrumbs)
cmake_minimum_required(VERSION 3.10)
enable_language(CXX)

find_package(Qt5 REQUIRED COMPONENTS Network)
find_package(ZLIB)

add_executable(HttpCompressingServer http/compressingserver.cpp)
target_link_libraries(HttpCompressingServer PUBLIC Qt5::Network ZLIB::ZLIB)
