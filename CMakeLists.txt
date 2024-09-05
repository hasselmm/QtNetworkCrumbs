cmake_minimum_required(VERSION 3.10)
project(QtNetworkCrumbs)
enable_testing()

set(CMAKE_CXX_STANDARD 17)
enable_language(CXX)

if (CMAKE_CXX_COMPILER_ID MATCHES GNU|Clang)
    set(QNC_COMPILE_OPTIONS
        -Wall -Wextra
        -Werror=conversion -Werror=missing-field-initializers -Werror=reorder
        -Werror=switch -Werror=uninitialized -Werror=unused)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 QUIET COMPONENTS Core NO_SYSTEM_ENVIRONMENT_PATH)

if (Qt6_FOUND)
    find_package(Qt6 REQUIRED COMPONENTS Network Test)
    find_package(Qt6 QUIET COMPONENTS Zlib)
    set(Qt_VERSION ${Qt6_VERSION})
    set(Qt_DIR ${Qt6_DIR})
else()
    find_package(Qt5 5.15 REQUIRED COMPONENTS Network Test)
    find_package(Qt5 QUIET COMPONENTS Zlib)
    set(Qt_VERSION ${Qt5_VERSION})
    set(Qt_DIR ${Qt5_DIR})
endif()

message(STATUS "Using Qt ${Qt_VERSION} from ${Qt_DIR}")

if (TARGET Qt::ZlibPrivate AND NOT ANDROID)
    # Qt::ZlibPrivate only provides headers, but no linkable symbols for Android
    message(STATUS "Using Qt::ZlibPrivate")
    add_library(QtNetworkCrumbs::Zlib ALIAS Qt::ZlibPrivate)
else()
    find_package(WrapZLIB QUIET)

    if (WrapZLIB_FOUND)
        add_library(QtNetworkCrumbs::Zlib ALIAS WrapZLIB::WrapZLIB)
    else()
        find_package(ZLIB REQUIRED)
        add_library(QtNetworkCrumbs::Zlib ALIAS ZLIB::ZLIB)
    endif()
endif()

add_custom_target(metadata SOURCES LICENSE README.md)

add_subdirectory(.github)
add_subdirectory(http)
add_subdirectory(mdns)
add_subdirectory(qnc)

if (NOT IOS) # FIXME Figure out code signing on Github
    add_subdirectory(tests)
endif()
