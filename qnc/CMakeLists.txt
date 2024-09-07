add_library(
    QtNetworkCrumbs STATIC
    qncliterals.h
    qncliterals.cpp
)

target_include_directories(QtNetworkCrumbs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(QtNetworkCrumbs PRIVATE ${QNC_COMPILE_OPTIONS})
target_link_libraries(QtNetworkCrumbs PUBLIC Qt::Network)

target_compile_definitions(
    QtNetworkCrumbs PRIVATE
    -DQT_DISABLE_DEPRECATED_BEFORE=0x050f00
    -DQT_NO_CAST_FROM_ASCII=1
    -DQT_NO_URL_CAST_FROM_STRING=1
)

