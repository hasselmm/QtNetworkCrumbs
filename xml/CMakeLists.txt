qnc_add_library(
    QncXml STATIC
    ALIAS Qnc::Xml

    xmlparser.cpp
    xmlparser.h
)

target_link_libraries(QncXml PUBLIC Qnc::Core)
