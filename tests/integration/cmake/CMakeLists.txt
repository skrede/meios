function(meios_cmake_case name)
    set(one_value    FIXTURE DRIVER ORIGIN REFUSES MATCHES BUILD_TARGET COMPONENT CONFIGS
                     MUTATE MUTATE_FROM MUTATE_BACKDATE TIMEOUT INSTALL CONSUMER
                     REQUIRE_PRESENT REQUIRE_ABSENT REQUIRE_CONTAINS REQUIRE_INSTALLED
                     REQUIRE_STABLE REQUIRE_PREFIX_EMPTY REQUIRE_PREFIX_ABSENT)
    set(multi_value  EXTRA SECOND_CONFIGURE CONSUMER_EXTRA)
    cmake_parse_arguments(ARG "" "${one_value}" "${multi_value}" ${ARGN})

    if(NOT ARG_DRIVER)
        set(ARG_DRIVER meios_cmake_case.cmake)
    endif()
    if(NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT 300)
    endif()

    # The work and cache directories are named after the case, which is what makes `ctest -j` safe
    # by construction and leaves a failed run's tree in a directory named after what failed.
    add_test(NAME ${name} COMMAND ${CMAKE_COMMAND}
        -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${ARG_FIXTURE}
        -DHARNESS_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DWORK=${CMAKE_CURRENT_BINARY_DIR}/work/${name}
        -DCACHE_DIR=${CMAKE_CURRENT_BINARY_DIR}/cache/${name}
        -DMODULE_DIR=${meios_SOURCE_DIR}/cmake
        -DGEN=${CMAKE_GENERATOR}
        -DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DGEN_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
        -DGEN_TOOLSET=${CMAKE_GENERATOR_TOOLSET}
        -DCONFIG=$<CONFIG>
        -DCONFIGS=${ARG_CONFIGS}
        -DORIGIN=${ARG_ORIGIN}
        -DBUILD_TARGET=${ARG_BUILD_TARGET}
        -DCOMPONENT=${ARG_COMPONENT}
        -DMUTATE=${ARG_MUTATE}
        -DMUTATE_FROM=${ARG_MUTATE_FROM}
        -DMUTATE_BACKDATE=${ARG_MUTATE_BACKDATE}
        -DINSTALL=${ARG_INSTALL}
        -DCONSUMER=${ARG_CONSUMER}
        "-DCONSUMER_EXTRA=${ARG_CONSUMER_EXTRA}"
        -DREQUIRE_STABLE=${ARG_REQUIRE_STABLE}
        -DREQUIRE_PRESENT=${ARG_REQUIRE_PRESENT}
        -DREQUIRE_ABSENT=${ARG_REQUIRE_ABSENT}
        -DREQUIRE_INSTALLED=${ARG_REQUIRE_INSTALLED}
        -DREQUIRE_PREFIX_EMPTY=${ARG_REQUIRE_PREFIX_EMPTY}
        "-DREQUIRE_PREFIX_ABSENT=${ARG_REQUIRE_PREFIX_ABSENT}"
        "-DREQUIRE_CONTAINS=${ARG_REQUIRE_CONTAINS}"
        "-DEXTRA=${ARG_EXTRA}"
        "-DSECOND_CONFIGURE=${ARG_SECOND_CONFIGURE}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/${ARG_DRIVER})

    # TIMEOUT is not optional: ctest applies its --timeout default to every case that lacks the
    # property, the Windows jobs run it at 60 seconds, and a sub-configure plus a build exceeds
    # that while being perfectly healthy. It is a defaulted keyword rather than a constant because
    # a case that configures and builds meios from scratch needs more than the shared default,
    # which every case that does neither must not be made to wait for.
    set_tests_properties(${name} PROPERTIES LABELS meios-cmake TIMEOUT ${ARG_TIMEOUT})

    # A refusal binds two properties rather than one. The pass regex alone would accept a run in
    # which the module never refused and the text arrived from somewhere else, so the sentinel the
    # driver prints when the sub-configure succeeded is bound as a failure regex to close that.
    if(ARG_REFUSES)
        set_tests_properties(${name} PROPERTIES
            PASS_REGULAR_EXPRESSION "${ARG_REFUSES}"
            FAIL_REGULAR_EXPRESSION "MEIOS_HARNESS_OK;MEIOS_HARNESS_ASSERT_FAILED")
    elseif(ARG_MATCHES)
        set_tests_properties(${name} PROPERTIES
            PASS_REGULAR_EXPRESSION "${ARG_MATCHES}"
            FAIL_REGULAR_EXPRESSION "MEIOS_HARNESS_FAILED;MEIOS_HARNESS_ASSERT_FAILED")
    endif()
endfunction()

# Globbed so a later group file registers itself by existing, the same way a test stem here
# registers only when its source is present.
file(GLOB _case_groups CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cases-*.cmake")
foreach(_group IN LISTS _case_groups)
    include("${_group}")
endforeach()
