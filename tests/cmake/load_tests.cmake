foreach(stem IN ITEMS load_error silent_default overload_resolution load_failure_propagation
        load_policy_refusal load_report load_arg_domain)
    set(load_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(EXISTS ${load_test_src})
        add_executable(${stem}_test ${load_test_src})
        target_link_libraries(${stem}_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf
                Catch2::Catch2WithMain)
        target_compile_definitions(${stem}_test PRIVATE
            MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf")
        meios_enable_coverage(${stem}_test)
        meios_warnings(${stem}_test)
        catch_discover_tests(${stem}_test TEST_PREFIX "${stem}.")
    endif()
endforeach()

foreach(stem IN ITEMS bundle_writer bundle_writer_pkg bundle_scanner bundle_checked_assets bundle_closure)
    set(bundle_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(EXISTS ${bundle_test_src})
        add_executable(${stem}_test ${bundle_test_src})
        target_link_libraries(${stem}_test
            PRIVATE meios::core meios::model meios::io meios::urdf meios::bundle
                Catch2::Catch2WithMain)
        target_compile_definitions(${stem}_test PRIVATE
            MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf")
        meios_enable_coverage(${stem}_test)
        meios_warnings(${stem}_test)
        catch_discover_tests(${stem}_test TEST_PREFIX "${stem}.")
    endif()
endforeach()

foreach(stem IN ITEMS urdf_load_sources)
    set(load_sources_src ${CMAKE_CURRENT_SOURCE_DIR}/integration/${stem}_test.cpp)
    if(EXISTS ${load_sources_src})
        add_executable(${stem}_test ${load_sources_src})
        target_link_libraries(${stem}_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf
                Catch2::Catch2WithMain)
        target_compile_definitions(${stem}_test PRIVATE
            MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf")
        meios_enable_coverage(${stem}_test)
        meios_warnings(${stem}_test)
        catch_discover_tests(${stem}_test)
    endif()
endforeach()
