add_library(MDNSResolver STATIC
    mdnsliterals.h
    mdnsliterals.cpp
    mdnsmessage.cpp
    mdnsmessage.h
    mdnsresolver.cpp
    mdnsresolver.h
    )

target_include_directories(MDNSResolver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MDNSResolver PUBLIC Qt::Network)

target_compile_definitions(MDNSResolver PRIVATE
    -DQT_DISABLE_DEPRECATED_BEFORE=0x050f00
    -DQT_NO_CAST_FROM_ASCII=1
    -DQT_NO_URL_CAST_FROM_STRING=1
    )

if (NOT IOS) # FIXME Figure out code signing on Github
    add_executable(MDNSResolverDemo mdnsresolverdemo.cpp)
    target_link_libraries(MDNSResolverDemo PRIVATE MDNSResolver)

    set_target_properties(MDNSResolverDemo PROPERTIES
        MACOSX_BUNDLE "de.taschenorakel.qnc.MDNSResolverDemo"
        XCODE_PRODUCT_TYPE "com.apple.product-type.bundle.unit-test"
        )
endif()

#if (WIN32)
#    add_executable(WinDNSTest windnstest.cpp)
#    target_compile_definitions(WinDNSTest PRIVATE -DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP)
#    target_link_libraries(WinDNSTest dnsapi.lib)
#endif()
