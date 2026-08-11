# The native load leg, owned entirely here so the consumer project beside it stays the small,
# readable program it is. Handed none of a pair's three inputs that pair does nothing, which is
# the configuration every existing continuous-integration leg on every platform uses. The two
# pairs are independently fail-closed: each is checked, configured and built under its own guard,
# and neither pair's inputs affect the other.

set(_ur3e_inputs MEIOS_CONSUMER_UR3E_DOCUMENT MEIOS_CONSUMER_UR3E_PACKAGE_ROOT MEIOS_CONSUMER_TESTS_DIR)
set(_kr6_inputs MEIOS_CONSUMER_KR6_DOCUMENT MEIOS_CONSUMER_KR6_PACKAGE_ROOT MEIOS_CONSUMER_TESTS_DIR)

set(_ur3e_wanted FALSE)
foreach(_input IN LISTS _ur3e_inputs)
    if(${_input})
        set(_ur3e_wanted TRUE)
    endif()
endforeach()

set(_kr6_wanted FALSE)
foreach(_input IN LISTS _kr6_inputs)
    if(${_input})
        set(_kr6_wanted TRUE)
    endif()
endforeach()

if(NOT _ur3e_wanted AND NOT _kr6_wanted)
    return()
endif()

if(_ur3e_wanted)
    foreach(_input IN LISTS _ur3e_inputs)
        if(NOT ${_input})
            message(FATAL_ERROR
                "the ur3e native load leg was asked for without ${_input}. A partially specified "
                "leg would be a leg that quietly does not exist, so all three of "
                "MEIOS_CONSUMER_UR3E_DOCUMENT, MEIOS_CONSUMER_UR3E_PACKAGE_ROOT and "
                "MEIOS_CONSUMER_TESTS_DIR are required together.")
        endif()
        if(NOT EXISTS "${${_input}}")
            message(FATAL_ERROR "${_input} names '${${_input}}', which does not exist.")
        endif()
    endforeach()
endif()

if(_kr6_wanted)
    foreach(_input IN LISTS _kr6_inputs)
        if(NOT ${_input})
            message(FATAL_ERROR
                "the kr6r900sixx native load leg was asked for without ${_input}. A partially "
                "specified leg would be a leg that quietly does not exist, so all three of "
                "MEIOS_CONSUMER_KR6_DOCUMENT, MEIOS_CONSUMER_KR6_PACKAGE_ROOT and "
                "MEIOS_CONSUMER_TESTS_DIR are required together.")
        endif()
        if(NOT EXISTS "${${_input}}")
            message(FATAL_ERROR "${_input} names '${${_input}}', which does not exist.")
        endif()
    endforeach()
endif()

# The fail-closed half of the absence claim: a consumer that could have linked the interpreter
# backend is not the one entitled to state that a load needs none.
if(TARGET meios::eval-python OR meios_eval-python_FOUND)
    message(FATAL_ERROR
        "meios::eval-python is reachable here, so this consumer cannot state that a pinned "
        "description loads with no interpreter. Configure meios with MEIOS_EVAL_PYTHON_SUPPORT "
        "off, or point this consumer at a package that does not carry it.")
endif()

# Judged from the only side that can judge it, and only on the route that has an installed package
# to judge. meios::yaml carries its third-party library on a private link edge which, on a static
# archive, survives in the exported interface as a link-only requirement -- so a package config
# that failed to resolve it would leave this consumer unable to link rather than failing at run
# time. That this file never finds it and never names it on a link line is the proof the config
# resolved it. The two acquisition paths of that dependency spell its target differently, so both
# spellings are accepted.
if(NOT MEIOS_CONSUMER_SOURCE_DIR)
    if(NOT meios_yaml_FOUND OR NOT TARGET meios::yaml)
        message(FATAL_ERROR
            "the installed meios does not carry the auxiliary-format component, so a pinned "
            "description cannot read the configuration it expands from.")
    endif()
    if(NOT TARGET yaml-cpp::yaml-cpp AND NOT TARGET yaml-cpp)
        message(FATAL_ERROR
            "the installed package config did not resolve the auxiliary-format module's own "
            "dependency, so linking against meios::yaml would fail.")
    endif()
endif()

if(_ur3e_wanted)
    # The variant and the record are one unit rather than two knobs: the facts asserted below were
    # measured from this variant alone, so a caller free to change one without the other would hold
    # a loaded model up against a record for a different robot.
    set(MEIOS_CONSUMER_UR_VARIANT ur3e)
    set(MEIOS_CONSUMER_UR3E_RECORD ur3e_facts.cases)

    configure_file(corpus_paths.h.in ${CMAKE_CURRENT_BINARY_DIR}/corpus_paths.h @ONLY)

    # No language standard here either, for the reason the project's other executable does not set one.
    add_executable(universal_robots_probe
        universal_robots_probe.cpp structure_facts.cpp limit_facts.cpp asset_facts.cpp)
    target_link_libraries(universal_robots_probe PRIVATE meios::urdf meios::bundle)
    target_include_directories(universal_robots_probe PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR} ${MEIOS_CONSUMER_TESTS_DIR})
    target_compile_definitions(universal_robots_probe PRIVATE
        MEIOS_GOLDEN_DIR="${MEIOS_CONSUMER_TESTS_DIR}/golden")

    add_test(NAME universal_robots COMMAND universal_robots_probe)

    # The same program again with an empty process path and no interpreter environment, so the claim
    # is about the run rather than only about the link line. The program reads the path back and
    # refuses if an interpreter is reachable after all.
    #
    # The argument arms the guard and the environment empties the path, deliberately from two
    # sources: an invocation that lost the property would otherwise leave a check that inspects
    # nothing and passes, rather than a check that reddens.
    add_test(NAME universal_robots_no_interpreter
        COMMAND universal_robots_probe --no-interpreter)
    set_tests_properties(universal_robots_no_interpreter PROPERTIES
        ENVIRONMENT "PATH=;PYTHONHOME=;PYTHONPATH=;MEIOS_CONSUMER_NO_INTERPRETER=1")
endif()

if(_kr6_wanted)
    # kr6r900sixx's document call takes no expansion argument at all, so this pair's record needs
    # no paired variant the way the ur3e pair's does.
    set(MEIOS_CONSUMER_KR6_RECORD kr6_facts.cases)

    configure_file(kr6_paths.h.in ${CMAKE_CURRENT_BINARY_DIR}/kr6_paths.h @ONLY)

    add_executable(kr6_probe
        kr6_probe.cpp model_facts.h structure_facts.cpp limit_facts.cpp asset_facts.cpp)
    target_link_libraries(kr6_probe PRIVATE meios::urdf meios::bundle)
    target_include_directories(kr6_probe PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR} ${MEIOS_CONSUMER_TESTS_DIR})
    target_compile_definitions(kr6_probe PRIVATE
        MEIOS_GOLDEN_DIR="${MEIOS_CONSUMER_TESTS_DIR}/golden")

    add_test(NAME kr6 COMMAND kr6_probe)

    add_test(NAME kr6_no_interpreter
        COMMAND kr6_probe --no-interpreter)
    set_tests_properties(kr6_no_interpreter PROPERTIES
        ENVIRONMENT "PATH=;PYTHONHOME=;PYTHONPATH=;MEIOS_CONSUMER_NO_INTERPRETER=1")
endif()
