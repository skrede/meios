cmake_minimum_required(VERSION 3.28)

include_guard(GLOBAL)

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

function(_meios_harness_config_args out)
    set(_args "")
    if(CONFIG)
        set(_args --config "${CONFIG}")
    endif()
    set(${out} "${_args}" PARENT_SCOPE)
endfunction()

# An empty target builds everything, which is what a case that has nothing to say about build order
# wants; naming one is how a case observes what a single rule pulls in behind it.
function(meios_harness_build binary target out_rc)
    _meios_harness_config_args(_args)
    if(target)
        list(APPEND _args --target "${target}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${binary}" ${_args}
        RESULT_VARIABLE _rc)
    set(${out_rc} "${_rc}" PARENT_SCOPE)
endfunction()

function(meios_harness_install binary prefix component out_rc)
    _meios_harness_config_args(_args)
    if(component)
        list(APPEND _args --component "${component}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --install "${binary}" --prefix "${prefix}" ${_args}
        RESULT_VARIABLE _rc)
    set(${out_rc} "${_rc}" PARENT_SCOPE)
endfunction()

# A case that binds a pass regex to a refusal matches it against the whole transcript, so an
# assertion failing afterwards would leave the case green with the failure printed underneath it.
# This token is bound as a failure regex on every case, which is what lets a refusal case assert
# anything about the tree at all.
function(meios_harness_failed_assertion text)
    message(FATAL_ERROR "MEIOS_HARNESS_ASSERT_FAILED ${text}")
endfunction()

# file(TOUCH) can only stamp the current time, so a modification time in the past has to come from
# the host's own tool. The read-back is part of the operation: a tool that silently declined would
# otherwise leave a case asserting nothing.
function(meios_harness_backdate path)
    if(CMAKE_HOST_WIN32)
        execute_process(
            COMMAND powershell -NoProfile -NonInteractive -Command
                    "(Get-Item -LiteralPath '${path}').LastWriteTime = [datetime]'2001-01-01T12:00:00Z'"
            RESULT_VARIABLE _rc)
    else()
        # -d with an explicit Z rather than -t, whose argument is read as local time and would land
        # the instant in the previous year for a host east of UTC. GNU and BSD touch both take it.
        execute_process(COMMAND touch -d 2001-01-01T12:00:00Z "${path}" RESULT_VARIABLE _rc)
    endif()
    if(NOT _rc EQUAL 0)
        meios_harness_failed_assertion("could not back-date ${path}")
    endif()
    meios_harness_require_year("${path}" 2001)
endfunction()

function(meios_harness_require_year path year)
    file(TIMESTAMP "${path}" _stamped "%Y" UTC)
    if(NOT _stamped STREQUAL "${year}")
        meios_harness_failed_assertion("${path} is stamped ${_stamped}, expected ${year}")
    endif()
endfunction()

function(meios_harness_require_dir path)
    if(NOT IS_DIRECTORY "${path}")
        meios_harness_failed_assertion("expected a directory at ${path}")
    endif()
endfunction()

function(meios_harness_require_file path)
    if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
        meios_harness_failed_assertion("expected a file at ${path}")
    endif()
endfunction()

function(meios_harness_require_absent path)
    if(EXISTS "${path}")
        meios_harness_failed_assertion("expected nothing at ${path}")
    endif()
endfunction()

# A prefix that was never created counts as empty, because an install that generated no rules never
# creates one. The glob is recursive and matches files only, so a prefix carrying nothing but empty
# directories is still judged on what it would hand a consumer.
function(meios_harness_require_prefix_empty prefix)
    file(GLOB_RECURSE _found "${prefix}/*")
    if(_found)
        meios_harness_failed_assertion("expected an empty prefix at ${prefix}, found ${_found}")
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
