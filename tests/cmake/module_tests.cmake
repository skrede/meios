# The module unit stems are wired but guarded: a stem becomes a real test the
# moment its source drops in, with no further CMake edit. meios::model is linked
# so a stem reaching the diagnostic seam through a module header compiles.
set(meios_module_test_stems
        xacro_eval xacro_scope xacro_property xacro_structural xacro_macro_params
        xacro_macro_defaults xacro_macro_redeclared xacro_macro_call xacro_deferred xacro_namespace xacro_block xacro_dollar_run xacro_lex_lookahead xacro_include_read xacro_include_find
        xacro_include_spelling xacro_include_bytes xacro_include_resolution
        xacro_include_precedence xacro_include_identity xacro_include_target xacro_subst
        xacro_subst_command xacro_subst_order xacro_binding
        xacro_budget xacro_depth xacro_arg xacro_arg_declaration xacro_arg_domain xacro_eval_command xacro_unsupported
        xacro_unsupported_location xacro_command_body
        xacro_corpus
        forward_scan
        yaml_resource yaml_resource_containment yaml_resource_followed yaml_grammar yaml_verdict io_concept
        io_source_stack io_sources io_package_join)
foreach(stem IN LISTS meios_module_test_stems)
    set(module_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(EXISTS ${module_test_src})
        add_executable(${stem}_test ${module_test_src})
        target_link_libraries(${stem}_test
            PRIVATE meios::core meios::model meios::xacro meios::io
                Catch2::Catch2WithMain)
        if(stem STREQUAL "xacro_corpus" OR stem STREQUAL "xacro_structural"
                OR stem STREQUAL "xacro_include_read")
            target_compile_definitions(${stem}_test PRIVATE
                MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        endif()
        # These stems pin what a probe document renders, so they read it where the differential
        # reads it, or where it still lives after leaving the differential's tables.
        if(stem STREQUAL "xacro_property" OR stem STREQUAL "xacro_macro_defaults"
                OR stem STREQUAL "xacro_block" OR stem STREQUAL "xacro_eval_command"
                OR stem STREQUAL "xacro_command_body" OR stem STREQUAL "xacro_include_resolution")
            target_compile_definitions(${stem}_test PRIVATE
                MEIOS_XACRO_PROBE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/xacro/probes")
        endif()
        # The forward-scan helper, the include-path helpers, the path grammar and the yaml resolver are
        # core-private, so these stems alone reach into meios-core/src; only forward-scan's helper takes
        # pugixml handles, so only it links pugixml directly.
        if(stem STREQUAL "forward_scan" OR stem STREQUAL "xacro_include_target" OR stem STREQUAL "io_package_join"
                OR stem STREQUAL "yaml_resource" OR stem STREQUAL "yaml_resource_containment"
                OR stem STREQUAL "yaml_resource_followed" OR stem STREQUAL "yaml_grammar" OR stem STREQUAL "yaml_verdict")
            target_include_directories(${stem}_test PRIVATE
                ${meios_SOURCE_DIR}/lib/meios-core/src)
        endif()
        if(stem STREQUAL "forward_scan")
            target_link_libraries(${stem}_test PRIVATE pugixml::pugixml)
        endif()
        if(stem STREQUAL "io_package_join")
            target_link_libraries(${stem}_test PRIVATE meios::bundle)
        endif()
        meios_enable_coverage(${stem}_test)
        meios_warnings(${stem}_test)
        catch_discover_tests(${stem}_test TEST_PREFIX "${stem}.")
    endif()
endforeach()
if(TARGET io_source_stack_test)
    target_sources(io_source_stack_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit/io_source_answers_test.cpp)
endif()
if(TARGET xacro_property_test)
    target_sources(xacro_property_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit/xacro_property_warning_test.cpp)
endif()
if(TARGET xacro_deferred_test)
    target_sources(xacro_deferred_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit/xacro_deferred_record_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unit/xacro_deferred_chain_test.cpp)
endif()
if(TARGET xacro_macro_call_test)
    target_sources(xacro_macro_call_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit/xacro_macro_lifetime_test.cpp)
endif()
if(TARGET xacro_include_read_test)
    target_sources(xacro_include_read_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit/xacro_include_warning_test.cpp)
endif()

# Catch discovery transports properties as name/value pairs, so a platform-specific
# scalar avoids splitting a semicolon-separated environment list into other properties.
set(scratch_unusable_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/scratch_unusable_test.cpp)
if(EXISTS ${scratch_unusable_test_src})
    set(absent_temp_dir ${CMAKE_CURRENT_BINARY_DIR}/absent-temporary-directory)
    add_executable(scratch_unusable_test ${scratch_unusable_test_src})
    target_link_libraries(scratch_unusable_test
        PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf
            Catch2::Catch2WithMain)
    target_compile_definitions(scratch_unusable_test PRIVATE
        MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf")
    meios_enable_coverage(scratch_unusable_test)
    meios_warnings(scratch_unusable_test)
    if(WIN32)
        set(absent_temp_environment "TMP=${absent_temp_dir}")
    else()
        set(absent_temp_environment "TMPDIR=${absent_temp_dir}")
    endif()
    catch_discover_tests(scratch_unusable_test
        TEST_PREFIX "scratch_unusable."
        PROPERTIES ENVIRONMENT "${absent_temp_environment}")
endif()

# The link this stem needs is a build-tree artifact rather than a fixture, because the environment
# it steers is process-wide and belongs to the registration. Creating a symbolic link needs a
# privilege Windows does not grant by default, so the condition lives here: a stem registered
# anyway would build a binary with no cases in it.
set(scratch_symlinked_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/scratch_symlinked_test.cpp)
if(NOT WIN32 AND EXISTS ${scratch_symlinked_test_src})
    set(symlinked_temp_target ${CMAKE_CURRENT_BINARY_DIR}/symlinked-temporary-directory-target)
    set(symlinked_temp_link ${CMAKE_CURRENT_BINARY_DIR}/symlinked-temporary-directory)
    file(MAKE_DIRECTORY ${symlinked_temp_target})
    file(REMOVE ${symlinked_temp_link})
    file(CREATE_LINK ${symlinked_temp_target} ${symlinked_temp_link} SYMBOLIC
        RESULT symlinked_temp_result)
    # A stem whose environment is a plain directory asserts nothing, so a link that could not be
    # created skips the registration rather than registering cases that would pass vacuously.
    if(symlinked_temp_result STREQUAL "0")
        add_executable(scratch_symlinked_test ${scratch_symlinked_test_src})
        target_link_libraries(scratch_symlinked_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf
                Catch2::Catch2WithMain)
        meios_enable_coverage(scratch_symlinked_test)
        meios_warnings(scratch_symlinked_test)
        catch_discover_tests(scratch_symlinked_test
            TEST_PREFIX "scratch_symlinked."
            PROPERTIES ENVIRONMENT "TMPDIR=${symlinked_temp_link}")
    endif()
endif()
