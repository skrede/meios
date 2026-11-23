if(TARGET meios_eval-python)
    set(module_path_first ${CMAKE_CURRENT_BINARY_DIR}/module-path-first)
    set(module_path_second ${CMAKE_CURRENT_BINARY_DIR}/module-path-second)
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
        # A ';' cannot reach a discovered case's environment, so Windows takes its empty element from a path-list
        # prepend; the interior discovery spells its selection apart because Catch names a discovery's files by its spec.
        if(WIN32)
            set(empty_path_element ENVIRONMENT "PYTHONPATH=${module_path_first}" ENVIRONMENT_MODIFICATION "PYTHONPATH=path_list_prepend:")
        else()
            set(empty_path_element ENVIRONMENT "PYTHONPATH=:")
        endif()
        catch_discover_tests(eval_python_start_test TEST_PREFIX "eval_python_start_empty_element." TEST_SPEC "[planted_module]"
            PROPERTIES ${empty_path_element})
        if(NOT WIN32)
            catch_discover_tests(eval_python_start_test TEST_PREFIX "eval_python_start_interior_element." TEST_SPEC "[eval_python][planted_module]"
                PROPERTIES ENVIRONMENT "PYTHONPATH=${module_path_first}::${module_path_second}")
        endif()
    endif()
    set(eval_python_host_state_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/eval_python_host_state_test.cpp)
    if(EXISTS ${eval_python_host_state_src})
        add_executable(eval_python_host_state_test ${eval_python_host_state_src})
        target_link_libraries(eval_python_host_state_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::eval-python
                pybind11::embed Catch2::Catch2WithMain)
        meios_enable_coverage(eval_python_host_state_test)
        meios_warnings(eval_python_host_state_test)
        catch_discover_tests(eval_python_host_state_test TEST_PREFIX "eval_python_host_state."
            PROPERTIES ENVIRONMENT_MODIFICATION "PYTHONPATH=unset:")
        # The module path is handed over after the preconfiguration, so the locale case runs again with one set.
        catch_discover_tests(eval_python_host_state_test TEST_PREFIX "eval_python_host_state_module_path." TEST_SPEC "[host_locale]"
            PROPERTIES ENVIRONMENT "PYTHONPATH=${module_path_first}")
        # Either variable can enable the fault handler, so the fatal-signal case runs again under each; the second
        # spells its selection apart because Catch names a discovery's files by its spec.
        catch_discover_tests(eval_python_host_state_test TEST_PREFIX "eval_python_host_state_faulthandler." TEST_SPEC "[fatal_signals]"
            PROPERTIES ENVIRONMENT "PYTHONFAULTHANDLER=1")
        catch_discover_tests(eval_python_host_state_test TEST_PREFIX "eval_python_host_state_dev_mode." TEST_SPEC "[eval_python][fatal_signals]"
            PROPERTIES ENVIRONMENT "PYTHONDEVMODE=1")
    endif()
    set(eval_python_closed_input_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/eval_python_closed_input_test.cpp)
    if(EXISTS ${eval_python_closed_input_src})
        add_executable(eval_python_closed_input_test ${eval_python_closed_input_src})
        target_link_libraries(eval_python_closed_input_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::eval-python
                pybind11::embed Catch2::Catch2WithMain)
        meios_enable_coverage(eval_python_closed_input_test)
        meios_warnings(eval_python_closed_input_test)
        catch_discover_tests(eval_python_closed_input_test TEST_PREFIX "eval_python_closed_input."
            PROPERTIES ENVIRONMENT_MODIFICATION "PYTHONPATH=unset:")
        catch_discover_tests(eval_python_closed_input_test TEST_PREFIX "eval_python_closed_input_during_start." TEST_SPEC "[closed_during_start]"
            PROPERTIES ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/python/closes_standard_input")
    endif()
    # A descriptor has a text or binary mode only on Windows.
    set(eval_python_stream_mode_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/eval_python_stream_mode_test.cpp)
    if(WIN32)
        if(EXISTS ${eval_python_stream_mode_src})
            add_executable(eval_python_stream_mode_test ${eval_python_stream_mode_src})
            target_link_libraries(eval_python_stream_mode_test
                PRIVATE meios::core meios::model meios::io meios::xacro meios::eval-python
                    pybind11::embed Catch2::Catch2WithMain)
            # The cases exist only where the start keeps the modes, which the guard's own header decides.
            target_include_directories(eval_python_stream_mode_test PRIVATE ${meios_SOURCE_DIR}/lib/meios-eval-python/src)
            meios_enable_coverage(eval_python_stream_mode_test)
            meios_warnings(eval_python_stream_mode_test)
            catch_discover_tests(eval_python_stream_mode_test TEST_PREFIX "eval_python_stream_mode."
                PROPERTIES ENVIRONMENT_MODIFICATION "PYTHONPATH=unset:")
        endif()
    endif()
endif()
