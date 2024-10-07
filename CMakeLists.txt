cmake_minimum_required(VERSION 3.10)
project(QtNetworkCrumbs)
enable_testing()

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(QncMacros)

set(CMAKE_CXX_STANDARD 17) # ------------------------------------------------------------------- setup compiler behavior
enable_language(CXX)

if (CMAKE_CXX_COMPILER_ID MATCHES GNU|Clang)
    set(QNC_COMPILE_OPTIONS
        -Wall -Wextra
        -Werror=conversion -Werror=missing-field-initializers -Werror=reorder
        -Werror=switch -Werror=uninitialized -Werror=unused)
endif()

if (NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(COMPILER_CACHE_EXECUTABLE sccache ccache)

    if (COMPILER_CACHE_EXECUTABLE)
        message(STATUS "Using compiler cache: ${COMPILER_CACHE_EXECUTABLE}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${COMPILER_CACHE_EXECUTABLE}")
    endif()
endif()

set(QNC_COMPILE_DEFINITIONS
    -DQT_NO_CAST_FROM_ASCII=1
    -DQT_NO_URL_CAST_FROM_STRING=1)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (NOT QT_DIR) # -------------------------------------------------------------------------------- find the Qt framework
    find_package(Qt6 QUIET COMPONENTS Core)
    set(QT_DIR "${Qt6_DIR}")
endif()

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core)

if (QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS Network Test)
    find_package(Qt6 QUIET COMPONENTS Zlib)

    if (QT_VERSION VERSION_GREATER_EQUAL 6.5)
        list(APPEND QNC_COMPILE_DEFINITIONS
            -DQT_DISABLE_DEPRECATED_UP_TO=0x060500)
    else()
        list(APPEND QNC_COMPILE_DEFINITIONS
            -DQT_DISABLE_DEPRECATED_BEFORE=0x060500)
    endif()

    set(Qt_DIR ${QT6_INSTALL_PREFIX})
    set(Qt_VERSION ${Qt6_VERSION})
elseif (QT_VERSION_MAJOR EQUAL 5)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Network Test)
    find_package(Qt5 QUIET COMPONENTS Zlib)

    list(APPEND QNC_COMPILE_DEFINITIONS
        -DQT_DISABLE_DEPRECATED_BEFORE=0x050f00)

    set(Qt_VERSION ${Qt5_VERSION})
    set(Qt_DIR ${Qt5_DIR})
else()
    message(FATAL_ERROR "Could NOT find Qt")
    return()
endif()

message(STATUS "Using Qt ${Qt_VERSION} from ${Qt_DIR}")

if (TARGET Qt::ZlibPrivate AND NOT ANDROID) # ----------------------------- figure out today's name of Qt's bundled zlib
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

add_custom_target(metadata SOURCES LICENSE README.md) # ------------------------------ define custom targets and subdirs

add_subdirectory(.github)
add_subdirectory(core)
add_subdirectory(http)
add_subdirectory(mdns)
add_subdirectory(ssdp)
add_subdirectory(xml)

if (NOT IOS) # FIXME Figure out code signing on Github
    add_subdirectory(tests)
endif()
