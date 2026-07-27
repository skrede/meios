cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/meios_cmake_harness.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/meios_cmake_origin.cmake")

function(prune_fail what)
    message(FATAL_ERROR "meios-prune-case: ${what}")
endfunction()

# An entry is a directory plus a sibling stamp file, and one glob matches both, so the two kinds are
# counted apart. Globbed rather than named: the suffix is a digest of an internal descriptor, and an
# assertion spelling one out would pin an implementation detail.
function(prune_expect pattern dirs files)
    file(GLOB _hits LIST_DIRECTORIES TRUE "${CACHE_DIR}/${pattern}")
    set(_dirs 0)
    set(_files 0)
    foreach(_hit IN LISTS _hits)
        if(IS_DIRECTORY "${_hit}")
            math(EXPR _dirs "${_dirs}+1")
        else()
            math(EXPR _files "${_files}+1")
        endif()
    endforeach()
    if(NOT _dirs EQUAL dirs OR NOT _files EQUAL files)
        string(REPLACE ";" "\n    " _list "${_hits}")
        set(_want "${dirs} director(ies) and ${files} file(s)")
        prune_fail("'${pattern}': expected ${_want}, found ${_dirs} and ${_files}:\n    ${_list}")
    endif()
endfunction()

# Located by the owner it records rather than by its name, which is a digest of that same path.
function(prune_claims_of tree out)
    set(_found "")
    file(GLOB _files "${CACHE_DIR}/*.claims")
    foreach(_file IN LISTS _files)
        file(STRINGS "${_file}" _owner REGEX "^owner=")
        if(_owner STREQUAL "owner=${tree}")
            set(_found "${_file}")
        endif()
    endforeach()
    set(${out} "${_found}" PARENT_SCOPE)
endfunction()

function(prune_entries tree prefix out)
    prune_claims_of("${tree}" _file)
    if(NOT _file)
        prune_fail("no claims file records ${tree} as its owner")
    endif()
    file(STRINGS "${_file}" _entries REGEX "^entry=${prefix}")
    set(${out} "${_entries}" PARENT_SCOPE)
endfunction()

function(prune_configure tree resources)
    set(_extra "-DFX_URL=${_url}" "-DFX_HASH=SHA256=${_hash}" "-DFX_RESOURCES=${resources}")
    meios_harness_configure("${FIXTURE}" "${tree}" "${_extra}" _rc)
    if(NOT _rc EQUAL 0)
        prune_fail("configuring ${tree} with '${resources}' exited ${_rc}")
    endif()
endfunction()

# The streams come back here because one assertion below is on the module's own report text rather
# than on a ctest regex; they are re-emitted so a failing run still shows what prune said.
function(prune_run flag out_text)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DMEIOS_RESOURCE_CACHE_DIR=${CACHE_DIR}" ${flag}
                -P "${MODULE_DIR}/MeiosPruneResources.cmake"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE  _err)
    message(STATUS "prune ${flag}:\n${_out}${_err}")
    if(NOT _rc EQUAL 0)
        prune_fail("the prune module exited ${_rc}")
    endif()
    if(out_text)
        set(${out_text} "${_out}${_err}" PARENT_SCOPE)
    endif()
endfunction()

meios_harness_reset()
meios_harness_tarball_origin("${HARNESS_DIR}/origin/clean" "${WORK}/origin.tar.gz" _url _hash)

set(_tree_a "${WORK}/treeA")
set(_tree_b "${WORK}/treeB")

prune_configure("${_tree_a}" "alpha,beta")
prune_expect("alpha-*" 1 1)
prune_expect("beta-*" 1 1)
prune_expect("*.claims" 0 1)

# One archive, two names, one shared cache: the second tree must land on the entry the first one
# acquired rather than acquire a second copy under another name. Asserted on the claimed entry
# itself and not only on the count, so a key that ever grew a per-build-tree component would be
# caught here rather than three steps later.
prune_configure("${_tree_b}" "alpha")
prune_expect("*.claims" 0 2)
prune_expect("alpha-*" 1 1)
prune_entries("${_tree_a}" "alpha-" _entries_a)
prune_entries("${_tree_b}" "alpha-" _entries_b)
if(NOT _entries_a OR NOT _entries_a STREQUAL _entries_b)
    prune_fail("the trees claim no one shared entry: '${_entries_a}' vs '${_entries_b}'")
endif()

prune_configure("${_tree_a}" "alpha")
prune_entries("${_tree_a}" "beta-" _stale_entries)
if(_stale_entries)
    prune_fail("tree A still claims ${_stale_entries} after dropping the declaration")
endif()

prune_run("" _report)
if(NOT _report MATCHES "Nothing was deleted")
    prune_fail("a reporting run did not say that nothing was deleted:\n${_report}")
endif()
prune_expect("alpha-*" 1 1)
prune_expect("beta-*" 1 1)
prune_expect("*.claims" 0 2)

prune_run(-DMEIOS_PRUNE_REMOVE=ON "")
prune_expect("beta-*" 0 0)
prune_expect("alpha-*" 1 1)
prune_expect("*.claims" 0 2)

# An owner whose cache file is gone has stopped claiming, and this is the leg that needs a second
# owner to mean anything: alpha survives only because tree A still declares it.
file(REMOVE "${_tree_b}/CMakeCache.txt")
prune_run(-DMEIOS_PRUNE_REMOVE=ON "")
prune_claims_of("${_tree_b}" _claims_b)
if(_claims_b)
    prune_fail("an owner with no CMakeCache.txt still holds a claims file: ${_claims_b}")
endif()
prune_expect("alpha-*" 1 1)

prune_configure("${_tree_a}" "")
prune_run(-DMEIOS_PRUNE_REMOVE=ON "")
prune_expect("*" 0 0)

meios_harness_sentinel(0)
