cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/meios_cmake_harness.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/meios_cmake_origin.cmake")

meios_harness_reset()

if(NOT ORIGIN STREQUAL "tarball")
    message(FATAL_ERROR "meios-harness: the multi-config driver serves ORIGIN tarball, not '${ORIGIN}'.")
endif()
meios_harness_tarball_origin("${HARNESS_DIR}/origin/clean" "${WORK}/origin.tar.gz" _url _hash)

# The generator is overridden rather than inherited, because whether the deployment follows the
# configuration is only observable where the generator carries more than one. No build type is
# selected: a multi-config generator takes it per build, and setting one here would be ignored on
# the way in and then mislead the reader.
set(GEN "Ninja Multi-Config")
set(CONFIG "")
meios_harness_configure("${FIXTURE}" "${WORK}/tree" "-DFX_URL=${_url};-DFX_HASH=SHA256=${_hash}" _rc)

string(REPLACE "," ";" _configs "${CONFIGS}")
foreach(_config IN LISTS _configs)
    if(NOT _rc EQUAL 0)
        break()
    endif()
    set(CONFIG "${_config}")
    meios_harness_build("${WORK}/tree" "" _rc)
endforeach()

if(_rc EQUAL 0)
    foreach(_config IN LISTS _configs)
        meios_harness_require_file("${WORK}/tree/run/${_config}/${REQUIRE_PRESENT}")
    endforeach()
endif()

meios_harness_sentinel(${_rc})
