function(add_testcase NAME)
    add_executable(${NAME} ${NAME}.cpp)
    add_test(NAME ${NAME} COMMAND $<TARGET_FILE:${NAME}>)
    target_link_libraries(${NAME} PRIVATE MDNSResolver Qt5::Test)

    set_target_properties(${NAME} PROPERTIES
        MACOSX_BUNDLE "de.taschenorakel.qnc.${NAME}"
        XCODE_PRODUCT_TYPE "com.apple.product-type.bundle.unit-test")
endfunction()

add_testcase(tst_mdnsmessages)
add_testcase(tst_mdnsresolver)
add_testcase(tst_mdnsmodels)
