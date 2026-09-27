include_guard(GLOBAL)

# A directory's target list is what an IDE shows as the project's targets.
function(meios_fixture_require_one_step target targets)
    get_property(_defined DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    if(NOT _defined STREQUAL targets)
        message(FATAL_ERROR "MEIOS_HARNESS_ASSERT_FAILED: the fixture defines targets "
                            "'${_defined}', expected '${targets}'")
    endif()
    get_target_property(_sources ${target} SOURCES)
    list(FILTER _sources INCLUDE REGEX "/CMakeFiles/${target}[.]meios$")
    list(LENGTH _sources _count)
    if(NOT _count EQUAL 1)
        message(FATAL_ERROR "MEIOS_HARNESS_ASSERT_FAILED: '${target}' carries ${_count} meios "
                            "steps, expected 1: '${_sources}'")
    endif()
    get_source_file_property(_symbolic "${_sources}" SYMBOLIC)
    if(NOT _symbolic)
        message(FATAL_ERROR "MEIOS_HARNESS_ASSERT_FAILED: '${_sources}' is not marked SYMBOLIC")
    endif()
endfunction()
