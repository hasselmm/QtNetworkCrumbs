if (NOT IOS) # FIXME Figure out code signing on Github
    add_executable(HttpCompressingServer compressingserver.cpp)
    target_link_libraries(HttpCompressingServer PUBLIC Qt5::Network)

    if (TARGET ZLIB::ZLIB)
        target_link_libraries(HttpCompressingServer PUBLIC ZLIB::ZLIB)
    elseif(TARGET Qt5::Zlib)
        target_link_libraries(HttpCompressingServer PUBLIC Qt5::Zlib)
    endif()

    set_target_properties(HttpCompressingServer PROPERTIES
        MACOSX_BUNDLE "de.taschenorakel.qnc.HttpCompressingServer"
        XCODE_PRODUCT_TYPE "com.apple.product-type.bundle.unit-test"
        )
endif()
