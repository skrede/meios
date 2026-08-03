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

foreach(stem IN ITEMS operation_failure operation_adapter text_reader io_source_lookup
                      load_acquisition yaml_acquisition scratch_setup scratch_publish
                      scratch_replace scratch_alias)
    meios_add_acquisition_test(${stem})
endforeach()

# Windows models directory permissions as a read-only attribute that denies nothing, so there is
# no denial to drive there; the registration carries the condition rather than the source, because
# a stem registered anyway would build a binary with no cases in it.
if(NOT WIN32)
    meios_add_acquisition_test(scratch_denial)
endif()

if(TARGET load_acquisition_test)
    target_compile_definitions(load_acquisition_test PRIVATE
        MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf")
endif()

# The CLI acquisition stem drives the verb bodies, so it takes the tool support lib the
# other CLI stems take; its own prefix keeps it selectable apart from them.
set(cli_acquisition_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/cli_acquisition_test.cpp)
if(TARGET meios_cli AND EXISTS ${cli_acquisition_src})
    add_executable(cli_acquisition_test ${cli_acquisition_src})
    target_link_libraries(cli_acquisition_test
        PRIVATE meios_cli meios::urdf meios::bundle meios::completion
            Catch2::Catch2WithMain)
    target_compile_definitions(cli_acquisition_test PRIVATE
        MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf"
        MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
    meios_enable_coverage(cli_acquisition_test)
    meios_warnings(cli_acquisition_test)
    catch_discover_tests(cli_acquisition_test TEST_PREFIX "cli_acquisition.")
endif()

if(TARGET yaml_acquisition_test AND TARGET meios_eval-python)
    target_link_libraries(yaml_acquisition_test PRIVATE meios::eval-python)
    target_compile_definitions(yaml_acquisition_test PRIVATE
        MEIOS_TEST_HAS_EVAL_PYTHON=1)
endif()
