qnc_add_library(
    QncCore STATIC

    abstractresolver.cpp
    abstractresolver.h
    literals.cpp
    literals.h
    multicastresolver.cpp
    multicastresolver.h
    parse.cpp
    parse.h
)

target_link_libraries(QncCore PUBLIC Qt::Network)
