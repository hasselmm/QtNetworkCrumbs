cmake_minimum_required(VERSION 3.10)
project(QtNetworkCrumbs)
enable_testing()

set(CMAKE_CXX_STANDARD 17)
enable_language(CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Network Test)
find_package(Qt5 QUIET COMPONENTS Zlib)

if (NOT TARGET Qt5::Zlib)
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
    add_library(Qt5::Zlib ALIAS ZLIB::ZLIB)
endif()

add_subdirectory(http)
add_subdirectory(mdns)
add_subdirectory(tests)
