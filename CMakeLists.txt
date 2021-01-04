cmake_minimum_required(VERSION 3.10)
project(QtNetworkCrumbs)
enable_testing()

set(CMAKE_CXX_STANDARD 17)
enable_language(CXX)

if (CMAKE_CXX_COMPILER_ID MATCHES GNU|Clang)
    string(APPEND CMAKE_CXX_FLAGS " -Wall -Wextra -Werror=conversion -Werror=reorder -Werror=switch -Werror=unused")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Network Test)
find_package(Qt5 QUIET COMPONENTS Zlib)

message(STATUS "Using Qt ${Qt5_VERSION} from ${Qt5_DIR}")

if (ANDROID OR NOT TARGET Qt5::Zlib)
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

    find_package(ZLIB REQUIRED)
    message(STATUS "Using Zlib ${ZLIB_VERSION_STRING} from ${ZLIB_LIBRARIES}")
else()
    message(STATUS "Using Zlib bundled with Qt ${Qt5_VERSION}")
endif()

add_subdirectory(.github)
add_subdirectory(http)
add_subdirectory(mdns)

if (NOT IOS) # FIXME Figure out code signing on Github
    add_subdirectory(tests)
endif()
