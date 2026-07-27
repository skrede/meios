cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/meios_cmake_harness.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/meios_cmake_origin.cmake")

# The bytes a mutation writes are fixed here rather than passed in, so a case names them in
# REQUIRE_CONTAINS and nothing has to travel twice.
set(_mutation "meios-harness-mutated")

meios_harness_reset()

if(NOT ORIGIN STREQUAL "tarball")
    message(FATAL_ERROR "meios-harness: the build driver serves ORIGIN tarball, not '${ORIGIN}'.")
endif()
meios_harness_tarball_origin("${HARNESS_DIR}/origin/clean" "${WORK}/origin.tar.gz" _url _hash)
set(_extra "-DFX_URL=${_url}" "-DFX_HASH=SHA256=${_hash}")

# What the driver derives goes ahead of EXTRA, so a case needing a different value for one of these
# simply repeats it in EXTRA and the later assignment wins.
list(APPEND _extra ${EXTRA})

meios_harness_configure("${FIXTURE}" "${WORK}/tree" "${_extra}" _rc)
if(_rc EQUAL 0)
    meios_harness_build("${WORK}/tree" _rc)
endif()

# The edit lands inside the acquired tree and touches no source file, which is the only way to tell
# a rule keyed on the tree's contents apart from one that merely runs when the target relinks.
if(_rc EQUAL 0 AND MUTATE)
    file(READ "${WORK}/tree/harness_tree.txt" _acquired)
    file(WRITE "${_acquired}/${MUTATE}" "${_mutation}\n")
    meios_harness_build("${WORK}/tree" _rc)
endif()

set(_run "${WORK}/tree/run")
if(_rc EQUAL 0)
    string(REPLACE "," ";" _present "${REQUIRE_PRESENT}")
    foreach(_rel IN LISTS _present)
        meios_harness_require_file("${_run}/${_rel}")
    endforeach()
    string(REPLACE "," ";" _absent "${REQUIRE_ABSENT}")
    foreach(_rel IN LISTS _absent)
        meios_harness_require_absent("${_run}/${_rel}")
    endforeach()
endif()

if(_rc EQUAL 0 AND REQUIRE_CONTAINS)
    string(REPLACE "," ";" _pair "${REQUIRE_CONTAINS}")
    list(GET _pair 0 _rel)
    list(GET _pair 1 _needle)
    meios_harness_require_file("${_run}/${_rel}")
    file(READ "${_run}/${_rel}" _deployed)
    if(NOT _deployed MATCHES "${_needle}")
        message(FATAL_ERROR "meios-harness: ${_rel} does not carry '${_needle}'")
    endif()
endif()

meios_harness_sentinel(${_rc})
