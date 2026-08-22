cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/meios_cmake_harness.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/meios_cmake_origin.cmake")

meios_harness_reset()

# The unsmudged tree is kept apart from the clean one because a single pointer anywhere under an
# acquired tree makes every acquisition of that tree refuse.
set(_extra "")
if(ORIGIN STREQUAL "git")
    meios_harness_git_origin("${HARNESS_DIR}/origin/clean" "${WORK}/origin" _url)
    set(_extra "-DFX_GIT_REPOSITORY=${_url}")
elseif(ORIGIN STREQUAL "git-lfs")
    meios_harness_git_origin("${HARNESS_DIR}/origin/lfs" "${WORK}/origin" _url)
    set(_extra "-DFX_GIT_REPOSITORY=${_url}")
elseif(ORIGIN STREQUAL "tarball")
    meios_harness_tarball_origin(
        "${HARNESS_DIR}/origin/clean" "${WORK}/origin.tar.gz" _url _hash)
    set(_extra "-DFX_URL=${_url}" "-DFX_HASH=SHA256=${_hash}")
endif()

# What the driver derives goes ahead of EXTRA, so a case needing a different value for one of these
# simply repeats it in EXTRA and the later assignment wins.
list(APPEND _extra ${EXTRA})

meios_harness_configure("${FIXTURE}" "${WORK}/tree" "${_extra}" _rc)

# One extra value rather than a step language: a case that has something to say about the second
# configure appends the values that differ, and the first configure's failure is reported as itself.
if(SECOND_CONFIGURE AND _rc EQUAL 0)
    list(APPEND _extra ${SECOND_CONFIGURE})
    meios_harness_configure("${FIXTURE}" "${WORK}/tree" "${_extra}" _rc)
endif()

meios_harness_sentinel(${_rc})
