cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/meios_cmake_harness.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/meios_cmake_origin.cmake")

meios_harness_reset()

if(NOT ORIGIN STREQUAL "tarball")
    message(FATAL_ERROR
        "meios-harness: the shared-directory driver serves ORIGIN tarball, not '${ORIGIN}'.")
endif()
meios_harness_tarball_origin("${HARNESS_DIR}/origin/clean" "${WORK}/origin.tar.gz" _url _hash)
set(_extra "-DFX_URL=${_url}" "-DFX_HASH=SHA256=${_hash}")
list(APPEND _extra ${EXTRA})

if(CONFIGS)
    meios_harness_select_ninja_multi_config()
endif()
meios_harness_configure("${FIXTURE}" "${WORK}/tree" "${_extra}" _rc)
if(CONFIGS)
    set(CONFIG "${CONFIGS}")
endif()

# One job per executable, so all three steps can start at once. MSBuild otherwise builds one project
# at a time, and a serial build never races.
set(ENV{CMAKE_BUILD_PARALLEL_LEVEL} 3)
set(_run "${WORK}/tree/run dir")
set(_suffix "")
if(CMAKE_HOST_WIN32)
    set(_suffix ".exe")
endif()

# The steps are attached to each executable rather than to a target of their own, so building one
# executable must build nothing else: its sibling binaries stay absent while its own lands.
if(_rc EQUAL 0)
    meios_harness_build("${WORK}/tree" app_a _rc)
endif()
if(_rc EQUAL 0)
    meios_harness_require_file("${_run}/app_a${_suffix}")
    meios_harness_require_absent("${_run}/app_b${_suffix}")
    meios_harness_require_absent("${_run}/app_c${_suffix}")
endif()

# Each round starts from an empty destination, so every round copies and publishes rather than
# compares, and repetition can only make a race likely, never prove one absent.
set(_failed 0)
foreach(_round RANGE 1 5)
    if(NOT _rc EQUAL 0)
        break()
    endif()
    file(REMOVE_RECURSE "${_run}/models" "${_run}/urdf")
    meios_harness_build("${WORK}/tree" "" _built)
    if(_built EQUAL 0)
        meios_harness_require_placement("${_run}" "${REQUIRE_PRESENT}" "${REQUIRE_CONTAINS}")
    else()
        math(EXPR _failed "${_failed} + 1")
    endif()
endforeach()
if(NOT _failed EQUAL 0)
    meios_harness_failed_assertion("${_failed} of 5 clean-destination builds failed")
endif()

meios_harness_sentinel(${_rc})
