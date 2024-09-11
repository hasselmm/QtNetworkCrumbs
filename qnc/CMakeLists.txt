qnc_add_library(
    QncCore STATIC
    qncliterals.cpp
    qncliterals.h
)

target_link_libraries(QncCore PUBLIC Qt::Network)
