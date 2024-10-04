qnc_add_library(
    QncCore STATIC
    ALIAS Qnc::Core

    abstractresolver.cpp
    abstractresolver.h
    compat.h
    detailmodel.cpp
    detailmodel.h
    literals.cpp
    literals.h
    multicastresolver.cpp
    multicastresolver.h
    parse.cpp
    parse.h
    treemodel.cpp
    treemodel.h
)

target_link_libraries(QncCore PUBLIC Qt::Network)
