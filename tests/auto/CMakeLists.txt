add_executable(tst_mdnsmessages tst_mdnsmessages.cpp)
target_link_libraries(tst_mdnsmessages PRIVATE MDNSResolver Qt5::Test)
add_test(NAME tst_mdnsmessages COMMAND $<TARGET_FILE:tst_mdnsmessages>)

set_target_properties(tst_mdnsmessages PROPERTIES
    MACOSX_BUNDLE "de.taschenorakel.qnc.tst_mdnsmessages"
    XCODE_PRODUCT_TYPE "com.apple.product-type.bundle.unit-test"
    )
