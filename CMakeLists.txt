project(QtNetworkCrumbs)
cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 14)
enable_language(CXX)

find_package(Qt5 REQUIRED COMPONENTS Network)
find_package(Qt5 QUIET COMPONENTS Zlib)

if (TARGET Qt5::Zlib)
    add_library(ZLIB::ZLIB ALIAS Qt5::Zlib)
else()
    foreach (dir IN LISTS CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
        if (EXISTS "${dir}/zlib.h")
            set(ZLIB_INCLUDE_DIR "${dir}")
            break()
        endif()
    endforeach()

    foreach (dir IN LISTS CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES)
        if (EXISTS "${dir}/libz.a")
            set(ZLIB_LIBRARY "${dir}/libz.a")
            break()
        endif()
    endforeach()

    find_package(ZLIB)
endif()

add_executable(HttpCompressingServer http/compressingserver.cpp)
target_link_libraries(HttpCompressingServer PUBLIC Qt5::Network ZLIB::ZLIB)

add_executable(MDnsServiceDiscovery mdns/servicediscovery.cpp)
target_link_libraries(MDnsServiceDiscovery PUBLIC Qt5::Network)
