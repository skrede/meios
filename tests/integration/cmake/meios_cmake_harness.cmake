cmake_minimum_required(VERSION 3.28)

function(meios_harness_reset)
    file(REMOVE_RECURSE "${WORK}")
    file(REMOVE_RECURSE "${CACHE_DIR}")
    file(MAKE_DIRECTORY "${WORK}")
endfunction()

# No OUTPUT_VARIABLE and no ERROR_VARIABLE: the child's streams have to reach ctest, because the
# pass and fail regexes registered on the case are the assertion, and capturing stderr here would
# swallow the very diagnostic a refusal case binds to.
function(meios_harness_configure source binary extra out_rc)
    set(_args "")
    # A Visual Studio generator leaves CMAKE_MAKE_PROGRAM empty; elsewhere the sub-configure would
    # rediscover a differently-installed ninja, or none.
    if(MAKE_PROGRAM)
        list(APPEND _args "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
    endif()
    if(GEN_PLATFORM)
        list(APPEND _args "-DCMAKE_GENERATOR_PLATFORM=${GEN_PLATFORM}")
    endif()
    if(GEN_TOOLSET)
        list(APPEND _args "-DCMAKE_GENERATOR_TOOLSET=${GEN_TOOLSET}")
    endif()
    if(CONFIG)
        list(APPEND _args "-DCMAKE_BUILD_TYPE=${CONFIG}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${source}" -B "${binary}" -G "${GEN}"
                "-DCMAKE_MODULE_PATH=${MODULE_DIR}"
                "-DMEIOS_RESOURCE_CACHE_DIR=${CACHE_DIR}"
                ${_args} ${extra}
        RESULT_VARIABLE _rc)
    set(${out_rc} "${_rc}" PARENT_SCOPE)
endfunction()

function(meios_harness_build binary out_rc)
    set(_config_args "")
    if(CONFIG)
        set(_config_args --config "${CONFIG}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${binary}" ${_config_args}
        RESULT_VARIABLE _rc)
    set(${out_rc} "${_rc}" PARENT_SCOPE)
endfunction()

function(meios_harness_require_dir path)
    if(NOT IS_DIRECTORY "${path}")
        message(FATAL_ERROR "meios-harness: expected a directory at ${path}")
    endif()
endfunction()

function(meios_harness_require_file path)
    if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
        message(FATAL_ERROR "meios-harness: expected a file at ${path}")
    endif()
endfunction()

function(meios_harness_require_absent path)
    if(EXISTS "${path}")
        message(FATAL_ERROR "meios-harness: expected nothing at ${path}")
    endif()
endfunction()

# Both sentinels are single whitespace-free tokens because CMake re-wraps message text at roughly
# 78 columns and breaks only at whitespace, so a token with no space in it survives wrapping intact
# and the regex a case binds to it stays exact.
#
# The failure branch is what gives a positive case a verdict at all: a driver that only printed a
# status would exit 0 whatever the sub-configure did, and every positive case would pass vacuously.
function(meios_harness_sentinel rc)
    if(rc EQUAL 0)
        message(STATUS "MEIOS_HARNESS_OK")
    else()
        message(FATAL_ERROR "MEIOS_HARNESS_FAILED rc=${rc}")
    endif()
endfunction()
