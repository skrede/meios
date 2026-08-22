if(TARGET meios_eval-python)
    set(eval_python_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/eval_python_test.cpp)
    if(EXISTS ${eval_python_src})
        add_executable(eval_python_test ${eval_python_src})
        target_link_libraries(eval_python_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::eval-python
                Catch2::Catch2WithMain)
        target_compile_definitions(eval_python_test PRIVATE
            MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        meios_enable_coverage(eval_python_test)
        meios_warnings(eval_python_test)
        catch_discover_tests(eval_python_test TEST_PREFIX "eval_python.")
    endif()

    set(eval_python_container_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/eval_python_container_test.cpp)
    if(EXISTS ${eval_python_container_src})
        add_executable(eval_python_container_test ${eval_python_container_src})
        target_link_libraries(eval_python_container_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::eval-python
                Catch2::Catch2WithMain)
        target_compile_definitions(eval_python_container_test PRIVATE
            MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        meios_enable_coverage(eval_python_container_test)
        meios_warnings(eval_python_container_test)
        catch_discover_tests(eval_python_container_test TEST_PREFIX "eval_python_container.")
    endif()

    set(eval_python_refusal_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/eval_python_refusal_test.cpp)
    if(EXISTS ${eval_python_refusal_src})
        add_executable(eval_python_refusal_test ${eval_python_refusal_src})
        target_link_libraries(eval_python_refusal_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::eval-python
                Catch2::Catch2WithMain)
        # The cardinality cases install the real YAML acquisition loader, which is a compiled
        # core-private symbol; a fake in its place could not observe the record it emits.
        target_include_directories(eval_python_refusal_test PRIVATE
            ${meios_SOURCE_DIR}/lib/meios-core/src)
        target_compile_definitions(eval_python_refusal_test PRIVATE
            MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        meios_enable_coverage(eval_python_refusal_test)
        meios_warnings(eval_python_refusal_test)
        # Catch2 registers a case under its prose name, so the stem is only selectable by
        # regex once it prefixes them.
        catch_discover_tests(eval_python_refusal_test TEST_PREFIX "eval_python_refusal.")
    endif()

    set(eval_python_yaml_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/eval_python_yaml_test.cpp)
    if(EXISTS ${eval_python_yaml_src})
        add_executable(eval_python_yaml_test ${eval_python_yaml_src})
        target_link_libraries(eval_python_yaml_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::eval-python
                Catch2::Catch2WithMain)
        # The acquisition cardinality cases install the real YAML loader, a compiled
        # core-private symbol; a fake in its place could not observe the record it emits.
        target_include_directories(eval_python_yaml_test PRIVATE
            ${meios_SOURCE_DIR}/lib/meios-core/src)
        target_compile_definitions(eval_python_yaml_test PRIVATE
            MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        meios_enable_coverage(eval_python_yaml_test)
        meios_warnings(eval_python_yaml_test)
        catch_discover_tests(eval_python_yaml_test TEST_PREFIX "eval_python_yaml.")
    endif()

    set(eval_python_unrestricted_src
        ${CMAKE_CURRENT_SOURCE_DIR}/unit/eval_python_unrestricted_test.cpp)
    if(EXISTS ${eval_python_unrestricted_src})
        add_executable(eval_python_unrestricted_test ${eval_python_unrestricted_src})
        target_link_libraries(eval_python_unrestricted_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::eval-python
                Catch2::Catch2WithMain)
        target_compile_definitions(eval_python_unrestricted_test PRIVATE
            MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        meios_enable_coverage(eval_python_unrestricted_test)
        meios_warnings(eval_python_unrestricted_test)
        catch_discover_tests(eval_python_unrestricted_test
            TEST_PREFIX "eval_python_unrestricted.")
    endif()

    set(eval_python_abuse_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/eval_python_abuse_test.cpp)
    if(EXISTS ${eval_python_abuse_src})
        add_executable(eval_python_abuse_test ${eval_python_abuse_src})
        target_link_libraries(eval_python_abuse_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::eval-python
                Catch2::Catch2WithMain)
        target_compile_definitions(eval_python_abuse_test PRIVATE
            MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        meios_enable_coverage(eval_python_abuse_test)
        meios_warnings(eval_python_abuse_test)
        catch_discover_tests(eval_python_abuse_test TEST_PREFIX "eval_python_abuse.")
    endif()

    set(eval_python_marker_input_src
        ${CMAKE_CURRENT_SOURCE_DIR}/unit/eval_python_marker_input_test.cpp)
    if(EXISTS ${eval_python_marker_input_src})
        add_executable(eval_python_marker_input_test ${eval_python_marker_input_src})
        # The caller-argument seam lives on the load path, so this stem reaches meios::urdf where
        # its siblings stop at the expansion seam.
        target_link_libraries(eval_python_marker_input_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf meios::eval-python
                Catch2::Catch2WithMain)
        target_compile_definitions(eval_python_marker_input_test PRIVATE
            MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        meios_enable_coverage(eval_python_marker_input_test)
        meios_warnings(eval_python_marker_input_test)
        catch_discover_tests(eval_python_marker_input_test
            TEST_PREFIX "eval_python_marker_input.")
    endif()

    set(eval_python_container_table_src
        ${CMAKE_CURRENT_SOURCE_DIR}/unit/eval_python_container_table_test.cpp)
    if(EXISTS ${eval_python_container_table_src})
        add_executable(eval_python_container_table_test ${eval_python_container_table_src})
        target_link_libraries(eval_python_container_table_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::eval-python
                Catch2::Catch2WithMain)
        target_compile_definitions(eval_python_container_table_test PRIVATE
            MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        meios_enable_coverage(eval_python_container_table_test)
        meios_warnings(eval_python_container_table_test)
        catch_discover_tests(eval_python_container_table_test
            TEST_PREFIX "eval_python_container_table.")
    endif()

    set(urdf_yaml_flatten_src ${CMAKE_CURRENT_SOURCE_DIR}/integration/urdf_yaml_flatten_test.cpp)
    if(EXISTS ${urdf_yaml_flatten_src})
        add_executable(urdf_yaml_flatten_test ${urdf_yaml_flatten_src})
        target_link_libraries(urdf_yaml_flatten_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf meios::eval-python
                Catch2::Catch2WithMain)
        # The flatten-to-text cases run the same expansion the load path drives, which needs
        # the real YAML acquisition loader, a compiled core-private symbol; a fake in its
        # place would resolve the configuration by rules of its own.
        target_include_directories(urdf_yaml_flatten_test PRIVATE
            ${meios_SOURCE_DIR}/lib/meios-core/src)
        target_compile_definitions(urdf_yaml_flatten_test PRIVATE
            MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf")
        # Real published descriptions are too large to vendor; point this at a local
        # checkout to turn the smoke cases live, leave it unset and they skip.
        if(MEIOS_SMOKE_CORPUS_DIR)
            target_compile_definitions(urdf_yaml_flatten_test PRIVATE
                MEIOS_SMOKE_CORPUS_DIR="${MEIOS_SMOKE_CORPUS_DIR}")
        endif()
        if(TARGET meios)
            target_compile_definitions(urdf_yaml_flatten_test PRIVATE
                MEIOS_CLI_BINARY="$<TARGET_FILE:meios>")
        endif()
        meios_enable_coverage(urdf_yaml_flatten_test)
        meios_warnings(urdf_yaml_flatten_test)
        catch_discover_tests(urdf_yaml_flatten_test TEST_PREFIX "urdf_yaml_flatten.")
    endif()
endif()
