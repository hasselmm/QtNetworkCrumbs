project(QtNetworkCrumbs)
cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 14)
enable_language(CXX)

find_package(Qt5 REQUIRED COMPONENTS Network)
find_package(ZLIB)

add_executable(HttpCompressingServer http/compressingserver.cpp)
target_link_libraries(HttpCompressingServer PUBLIC Qt5::Network ZLIB::ZLIB)

add_executable(MDnsServiceDiscovery mdns/servicediscovery.cpp)
target_link_libraries(MDnsServiceDiscovery PUBLIC Qt5::Network)
