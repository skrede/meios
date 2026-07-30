set(operation_failure_test_src
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/operation_failure_test.cpp)
if(EXISTS ${operation_failure_test_src})
    add_executable(operation_failure_test ${operation_failure_test_src})
    target_include_directories(operation_failure_test PRIVATE
        ${CMAKE_SOURCE_DIR}/lib/meios-core/src)
    target_link_libraries(operation_failure_test
        PRIVATE meios::core meios::model meios::io Catch2::Catch2WithMain)
    meios_enable_coverage(operation_failure_test)
    meios_warnings(operation_failure_test)
    catch_discover_tests(operation_failure_test TEST_PREFIX "operation_failure.")
endif()

set(text_reader_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/text_reader_test.cpp)
if(EXISTS ${text_reader_test_src})
    add_executable(text_reader_test ${text_reader_test_src})
    target_include_directories(text_reader_test PRIVATE
        ${CMAKE_SOURCE_DIR}/lib/meios-core/src)
    target_link_libraries(text_reader_test
        PRIVATE meios::core meios::model meios::io Catch2::Catch2WithMain)
    meios_enable_coverage(text_reader_test)
    meios_warnings(text_reader_test)
    catch_discover_tests(text_reader_test TEST_PREFIX "text_reader.")
endif()
