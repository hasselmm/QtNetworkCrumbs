if (NOT IOS) # FIXME Figure out code signing on Github
    add_executable(HttpCompressingServer compressingserver.cpp)
    target_compile_options(HttpCompressingServer PRIVATE ${QNC_COMPILE_OPTIONS})
    target_link_libraries(HttpCompressingServer PUBLIC Qt::Network QtNetworkCrumbs::Zlib)

    set_target_properties(
        HttpCompressingServer PROPERTIES
        MACOSX_BUNDLE "de.taschenorakel.qnc.HttpCompressingServer"
        XCODE_PRODUCT_TYPE "com.apple.product-type.bundle.unit-test"
    )
endif()
