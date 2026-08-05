# The module unit stems are wired but guarded: a stem becomes a real test the
# moment its source drops in, with no further CMake edit. meios::model is linked
# so a stem reaching the diagnostic seam through a module header compiles.
foreach(stem IN ITEMS
        xacro_eval xacro_scope xacro_structural xacro_macro_params xacro_include_read xacro_subst
        xacro_subst_command
        xacro_budget xacro_arg xacro_unsupported xacro_corpus forward_scan
        yaml_resource yaml_grammar yaml_verdict io_concept io_source_stack io_sources)
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
        # The forward-scan helper and the yaml resolver are compiled core-private symbols,
        # so these stems alone reach into meios-core/src; only forward-scan's helper takes
        # pugixml handles, so only it links pugixml directly.
        if(stem STREQUAL "forward_scan" OR stem STREQUAL "yaml_resource"
                OR stem STREQUAL "yaml_grammar" OR stem STREQUAL "yaml_verdict")
            target_include_directories(${stem}_test PRIVATE
                ${meios_SOURCE_DIR}/lib/meios-core/src)
        endif()
        if(stem STREQUAL "forward_scan")
            target_link_libraries(${stem}_test PRIVATE pugixml::pugixml)
        endif()
        meios_enable_coverage(${stem}_test)
        meios_warnings(${stem}_test)
        catch_discover_tests(${stem}_test TEST_PREFIX "${stem}.")
    endif()
endforeach()

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
