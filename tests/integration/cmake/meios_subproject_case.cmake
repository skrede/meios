cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/meios_cmake_harness.cmake")

meios_harness_reset()

if(ORIGIN)
    message(FATAL_ERROR
        "meios-subproject-case: the subproject driver acquires nothing and serves no origin, "
        "so ORIGIN '${ORIGIN}' has no meaning here.")
endif()

set(_extra "${EXTRA}")
meios_harness_configure("${FIXTURE}" "${WORK}/tree" "${_extra}" _rc)

# One extra value rather than a step language, as in the default driver: a case with something to
# say about the second configure appends the values that differ, and the first failure is its own.
if(SECOND_CONFIGURE AND _rc EQUAL 0)
    list(APPEND _extra ${SECOND_CONFIGURE})
    meios_harness_configure("${FIXTURE}" "${WORK}/tree" "${_extra}" _rc)
endif()

if(_rc EQUAL 0 AND BUILD_TARGET)
    meios_harness_build("${WORK}/tree" "${BUILD_TARGET}" _rc)
endif()

# The install trigger is separate from what is asserted, which is the whole reason this driver
# exists: "install, then assert the prefix is empty" has no spelling where the two are one value.
set(_prefix "${WORK}/prefix")
if(_rc EQUAL 0 AND INSTALL)
    meios_harness_install("${WORK}/tree" "${_prefix}" "${COMPONENT}" _rc)
endif()

if(_rc EQUAL 0)
    string(REPLACE "," ";" _installed "${REQUIRE_INSTALLED}")
    foreach(_rel IN LISTS _installed)
        meios_harness_require_file("${_prefix}/${_rel}")
    endforeach()
    string(REPLACE "," ";" _absent "${REQUIRE_PREFIX_ABSENT}")
    foreach(_rel IN LISTS _absent)
        meios_harness_require_absent("${_prefix}/${_rel}")
    endforeach()
    if(REQUIRE_PREFIX_EMPTY)
        meios_harness_require_prefix_empty("${_prefix}")
    endif()
endif()

# Prefix-relative, unlike the build driver's, because what an installed package records about
# itself is a file in the prefix and is readable nowhere else.
if(_rc EQUAL 0 AND REQUIRE_CONTAINS)
    string(REPLACE "," ";" _pair "${REQUIRE_CONTAINS}")
    list(GET _pair 0 _rel)
    list(GET _pair 1 _needle)
    meios_harness_require_file("${_prefix}/${_rel}")
    file(READ "${_prefix}/${_rel}" _text)
    if(NOT _text MATCHES "${_needle}")
        meios_harness_failed_assertion("${_rel} does not carry '${_needle}'")
    endif()
endif()

# What a prefix carries is a file listing; what it hands a consumer is a find_package. Only a
# second configure against the staged prefix can judge the second, so a case that has something to
# say about it names the project to point at that prefix.
if(_rc EQUAL 0 AND CONSUMER)
    set(_consumer_args "-DCMAKE_PREFIX_PATH=${_prefix}" ${CONSUMER_EXTRA})
    meios_harness_configure("${HARNESS_DIR}/fixtures/${CONSUMER}" "${WORK}/consumer"
        "${_consumer_args}" _rc)
endif()

meios_harness_sentinel(${_rc})
