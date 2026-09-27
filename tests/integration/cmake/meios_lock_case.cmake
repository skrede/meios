cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/meios_cmake_harness.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/meios_cmake_origin.cmake")
include("${MODULE_DIR}/MeiosStepLock.cmake")

meios_harness_reset()

if(NOT ORIGIN STREQUAL "tarball")
    message(FATAL_ERROR "meios-harness: the lock driver serves ORIGIN tarball, not '${ORIGIN}'.")
endif()
meios_harness_tarball_origin("${HARNESS_DIR}/origin/clean" "${WORK}/origin.tar.gz" _url _hash)
set(_extra "-DFX_URL=${_url}" "-DFX_HASH=SHA256=${_hash}")
# The bound is fixed here rather than passed in, so the configure and the elapsed-time check agree.
set(_bound 2)
list(APPEND _extra "-DMEIOS_STEP_LOCK_TIMEOUT=${_bound}" ${EXTRA})

meios_harness_configure("${FIXTURE}" "${WORK}/tree" "${_extra}" _rc)
if(_rc EQUAL 0)
    file(STRINGS "${WORK}/tree/harness_lock.txt" _lock_inputs)
    list(GET _lock_inputs 0 _binary)
    list(GET _lock_inputs 1 _root)
    _meios_step_lock("${_binary}" "${_root}" "" _lock)
    string(TIMESTAMP _start "%s")
    meios_harness_build("${WORK}/tree" "" _held_rc)
    string(TIMESTAMP _end "%s")
    math(EXPR _waited "${_end} - ${_start}")
    # Second-granular timestamps still bound the wait from below, since a step that waited two
    # seconds cannot read as less.
    if(_held_rc EQUAL 0)
        meios_harness_failed_assertion("the build finished while the lock of ${_root} was held")
    elseif(_waited LESS _bound)
        meios_harness_failed_assertion(
            "the build gave up after ${_waited} seconds, before its ${_bound}-second bound")
    endif()
    file(LOCK "${_lock}" RELEASE)
    meios_harness_build("${WORK}/tree" "" _rc)
endif()
if(_rc EQUAL 0)
    meios_harness_require_placement(
        "${WORK}/tree/run dir" "${REQUIRE_PRESENT}" "${REQUIRE_CONTAINS}")
endif()

meios_harness_sentinel(${_rc})
