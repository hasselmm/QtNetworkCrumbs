# ----------------------------------------------------------------------------------------------------------------------
# Defines a new library and common properties.
# Besides the convenience an adoption to older Qt versions not providing qt_add_library().
# ----------------------------------------------------------------------------------------------------------------------
function(qnc_add_library NAME)
    cmake_parse_arguments(LIBRARY "" "ALIAS" "" ${ARGN})

    if (NOT LIBRARY_ALIAS)
        message(FATAL_ERROR "No alias defined for ${NAME}")
    endif()

    if (COMMAND qt_add_library)
        qt_add_library("${NAME}" ${LIBRARY_UNPARSED_ARGUMENTS})
    else()
        add_library("${NAME}" ${LIBRARY_UNPARSED_ARGUMENTS})
    endif()

    target_include_directories("${NAME}" PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions("${NAME}" PRIVATE ${QNC_COMPILE_DEFINITIONS})
    target_compile_options    ("${NAME}" PRIVATE ${QNC_COMPILE_OPTIONS})

    add_library("${LIBRARY_ALIAS}" ALIAS "${NAME}")
endfunction()

# ----------------------------------------------------------------------------------------------------------------------
# Defines a new executable and common properties.
# Besides the convenience an adoption to older Qt versions not providing qnc_add_executable().
# ----------------------------------------------------------------------------------------------------------------------
function(qnc_add_executable NAME)
    cmake_parse_arguments(EXECUTABLE "" "TYPE" "" ${ARGN})

    if (COMMAND qt_add_executable)
        qt_add_executable("${NAME}" ${EXECUTABLE_UNPARSED_ARGUMENTS})
    else()
        add_executable("${NAME}" ${EXECUTABLE_UNPARSED_ARGUMENTS})
    endif()

    target_compile_definitions("${NAME}" PRIVATE ${QNC_COMPILE_DEFINITIONS})
    target_compile_options    ("${NAME}" PRIVATE ${QNC_COMPILE_OPTIONS})

    set_target_properties(
        "${NAME}" PROPERTIES
        MACOSX_BUNDLE "de.taschenorakel.qnc.${NAME}"
        XCODE_PRODUCT_TYPE "com.apple.product-type.bundle.${EXECUTABLE_TYPE}"
    )
endfunction()

# ----------------------------------------------------------------------------------------------------------------------
# Defines a new test executable and registers it to CTest.
# ----------------------------------------------------------------------------------------------------------------------
function(add_testcase SOURCE_FILENAME) # [SOURCES...]
    cmake_parse_arguments(TESTCASE "" "NAME" "LIBRARIES;SOURCES" ${ARGN})

    if (NOT TESTCASE_NAME)
        cmake_path(GET SOURCE_FILENAME STEM TESTCASE_NAME)
    endif()

    list(PREPEND TESTCASE_SOURCES "${SOURCE_FILENAME}")
    list(APPEND  TESTCASE_SOURCES ${TESTCASE_UNPARSED_ARGUMENTS})

    qnc_add_executable("${TESTCASE_NAME}" TYPE "unit-test" ${TESTCASE_SOURCES})
    target_link_libraries("${TESTCASE_NAME}" PRIVATE Qt::Test ${TESTCASE_LIBRARIES})
    add_test(NAME "${TESTCASE_NAME}" COMMAND ${TEST_RUNNER} "$<TARGET_FILE:${TESTCASE_NAME}>")
endfunction()
