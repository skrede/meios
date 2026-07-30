function(meios_add_acquisition_test stem)
    set(test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(NOT EXISTS ${test_src})
        return()
    endif()
    add_executable(${stem}_test ${test_src})
    target_include_directories(${stem}_test PRIVATE
        ${CMAKE_SOURCE_DIR}/lib/meios-core/src)
    target_link_libraries(${stem}_test PRIVATE
        meios::core meios::model meios::io meios::urdf meios::xacro
        Catch2::Catch2WithMain)
    meios_enable_coverage(${stem}_test)
    meios_warnings(${stem}_test)
    catch_discover_tests(${stem}_test TEST_PREFIX "${stem}.")
endfunction()

foreach(stem IN ITEMS operation_failure text_reader io_source_lookup
                      load_acquisition yaml_acquisition)
    meios_add_acquisition_test(${stem})
endforeach()

if(TARGET load_acquisition_test)
    target_compile_definitions(load_acquisition_test PRIVATE
        MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf")
endif()

if(TARGET yaml_acquisition_test AND TARGET meios_eval-python)
    target_link_libraries(yaml_acquisition_test PRIVATE meios::eval-python)
    target_compile_definitions(yaml_acquisition_test PRIVATE
        MEIOS_TEST_HAS_EVAL_PYTHON=1)
endif()
