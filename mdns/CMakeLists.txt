add_library(MDNSResolver STATIC
    mdnshostlistmodel.cpp
    mdnshostlistmodel.h
    mdnsmessage.cpp
    mdnsmessage.h
    mdnsmodelsupport.cpp
    mdnsmodelsupport.h
    mdnsresolver.cpp
    mdnsresolver.h
    mdnsservicelistmodel.cpp
    mdnsservicelistmodel.h
    )

target_include_directories(MDNSResolver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MDNSResolver PUBLIC Qt5::Network)

target_compile_definitions(MDNSResolver PRIVATE
    -DQT_DISABLE_DEPRECATED_BEFORE=0x050f00
    -DQT_RESTRICTED_CAST_FROM_ASCII=1
    -DQT_NO_URL_CAST_FROM_STRING=1
    )

if (NOT IOS) # FIXME Figure out code signing on Github
    add_executable(MDNSResolverDemo mdnsresolverdemo.cpp)
    target_link_libraries(MDNSResolverDemo PRIVATE MDNSResolver)

    set_target_properties(MDNSResolverDemo PROPERTIES
        MACOSX_BUNDLE "de.taschenorakel.qnc.MDNSResolverDemo"
        XCODE_PRODUCT_TYPE "com.apple.product-type.bundle.unit-test"
        )

    add_executable(MDNSModelDemo mdnsmodeldemo.cpp)
    target_link_libraries(MDNSModelDemo PRIVATE MDNSResolver Qt5::Widgets)

    set_target_properties(MDNSModelDemo PROPERTIES
        MACOSX_BUNDLE "de.taschenorakel.qnc.MDNSModelDemo"
        )
endif()

#if (WIN32)
#    add_executable(WinDNSTest windnstest.cpp)
#    target_compile_definitions(WinDNSTest PRIVATE -DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP)
#    target_link_libraries(WinDNSTest dnsapi.lib)
#endif()
