function(meios_cmake_case name)
    set(one_value    FIXTURE DRIVER ORIGIN REFUSES MATCHES)
    set(multi_value  EXTRA)
    cmake_parse_arguments(ARG "" "${one_value}" "${multi_value}" ${ARGN})

    if(NOT ARG_DRIVER)
        set(ARG_DRIVER meios_cmake_case.cmake)
    endif()

    # The work and cache directories are named after the case, which is what makes `ctest -j` safe
    # by construction and leaves a failed run's tree in a directory named after what failed.
    add_test(NAME ${name} COMMAND ${CMAKE_COMMAND}
        -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${ARG_FIXTURE}
        -DHARNESS_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/work/${name}
        -DCACHE_DIR=${CMAKE_CURRENT_BINARY_DIR}/cache/${name}
        -DMODULE_DIR=${CMAKE_SOURCE_DIR}/cmake
        -DGEN=${CMAKE_GENERATOR}
        -DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DGEN_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
        -DGEN_TOOLSET=${CMAKE_GENERATOR_TOOLSET}
        -DCONFIG=$<CONFIG>
        -DORIGIN=${ARG_ORIGIN}
        "-DEXTRA=${ARG_EXTRA}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/${ARG_DRIVER})

    # TIMEOUT is not optional: ctest applies its --timeout default to every case that lacks the
    # property, the Windows jobs run it at 60 seconds, and a sub-configure plus a build exceeds
    # that while being perfectly healthy.
    set_tests_properties(${name} PROPERTIES LABELS meios-cmake TIMEOUT 300)

    # A refusal binds two properties rather than one. The pass regex alone would accept a run in
    # which the module never refused and the text arrived from somewhere else, so the sentinel the
    # driver prints when the sub-configure succeeded is bound as a failure regex to close that.
    if(ARG_REFUSES)
        set_tests_properties(${name} PROPERTIES
            PASS_REGULAR_EXPRESSION "${ARG_REFUSES}"
            FAIL_REGULAR_EXPRESSION "MEIOS_HARNESS_OK")
    elseif(ARG_MATCHES)
        set_tests_properties(${name} PROPERTIES
            PASS_REGULAR_EXPRESSION "${ARG_MATCHES}"
            FAIL_REGULAR_EXPRESSION "MEIOS_HARNESS_FAILED")
    endif()
endfunction()

# Globbed so a later group file registers itself by existing, the same way a test stem here
# registers only when its source is present.
file(GLOB _case_groups CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cases-*.cmake")
foreach(_group IN LISTS _case_groups)
    include("${_group}")
endforeach()
