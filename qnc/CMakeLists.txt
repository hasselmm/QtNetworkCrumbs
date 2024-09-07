qnc_add_library(
    QtNetworkCrumbs STATIC
    qncliterals.h
    qncliterals.cpp
)

target_link_libraries(QtNetworkCrumbs PUBLIC Qt::Network)
