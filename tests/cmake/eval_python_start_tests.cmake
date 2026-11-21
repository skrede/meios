if(TARGET meios_eval-python)
    set(eval_python_start_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/eval_python_start_test.cpp)
    if(EXISTS ${eval_python_start_src})
        add_executable(eval_python_start_test ${eval_python_start_src})
        target_link_libraries(eval_python_start_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::eval-python
                pybind11::embed Catch2::Catch2WithMain)
        target_include_directories(eval_python_start_test PRIVATE
            ${meios_SOURCE_DIR}/lib/meios-eval-python/src
            ${meios_SOURCE_DIR}/lib/meios-core/src)
        meios_enable_coverage(eval_python_start_test)
        meios_warnings(eval_python_start_test)
        # A start happens once per process, so the failing starts each run under an environment of their own.
        catch_discover_tests(eval_python_start_test TEST_PREFIX "eval_python_start." TEST_SPEC "~[start_failure]~[pin_failure]")
        catch_discover_tests(eval_python_start_test TEST_PREFIX "eval_python_start_failed." TEST_SPEC "[start_failure]"
            PROPERTIES ENVIRONMENT "PYTHONHOME=${CMAKE_CURRENT_BINARY_DIR}/no-python-home")
        catch_discover_tests(eval_python_start_test TEST_PREFIX "eval_python_pin_failed." TEST_SPEC "[pin_failure]"
            PROPERTIES ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/python/digit_limit_refused")
    endif()
endif()
