qnc_add_library(
    QncXml STATIC
    xmlparser.cpp
    xmlparser.h
)

target_link_libraries(QncXml PUBLIC QncCore)
