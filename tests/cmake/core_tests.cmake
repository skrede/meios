foreach(stem IN ITEMS math diag records types sink)
    set(model_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/model_${stem}_test.cpp)
    if(EXISTS ${model_test_src})
        add_executable(model_${stem}_test ${model_test_src})
        target_link_libraries(model_${stem}_test
            PRIVATE meios::core meios::model Catch2::Catch2WithMain)
        meios_enable_coverage(model_${stem}_test)
        meios_warnings(model_${stem}_test)
        catch_discover_tests(model_${stem}_test TEST_PREFIX "model_${stem}.")
    endif()
endforeach()

foreach(stem IN ITEMS world_recorder)
    set(recorder_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(EXISTS ${recorder_test_src})
        add_executable(${stem}_test ${recorder_test_src})
        target_link_libraries(${stem}_test
            PRIVATE meios::core meios::model Catch2::Catch2WithMain)
        meios_enable_coverage(${stem}_test)
        meios_warnings(${stem}_test)
        catch_discover_tests(${stem}_test)
    endif()
endforeach()

# The expected vocabulary aliases std::expected where the standard library ships it
# and a C++20 fallback otherwise. Both branches carry the same call surface, so one
# source proves both: expected_std_test pins C++23 to reach std::expected, and
# expected_fallback_test forces the fallback even where std::expected exists.
set(expected_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/expected_test.cpp)
if(EXISTS ${expected_test_src})
    add_executable(expected_std_test ${expected_test_src})
    target_compile_features(expected_std_test PRIVATE cxx_std_23)
    target_link_libraries(expected_std_test
        PRIVATE meios::core meios::model meios::xacro Catch2::Catch2WithMain)
    meios_enable_coverage(expected_std_test)
    meios_warnings(expected_std_test)
    catch_discover_tests(expected_std_test TEST_PREFIX "std.")

    add_executable(expected_fallback_test ${expected_test_src})
    target_compile_definitions(expected_fallback_test PRIVATE MEIOS_EXPECTED_FORCE_FALLBACK)
    target_link_libraries(expected_fallback_test
        PRIVATE meios::core meios::model meios::xacro Catch2::Catch2WithMain)
    meios_enable_coverage(expected_fallback_test)
    meios_warnings(expected_fallback_test)
    catch_discover_tests(expected_fallback_test TEST_PREFIX "fallback.")
endif()
