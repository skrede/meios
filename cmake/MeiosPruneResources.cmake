# Collects acquired resource trees that no build tree asks for any more. Run in script mode:
#
#   cmake -DMEIOS_RESOURCE_CACHE_DIR=<dir> [-DMEIOS_PRUNE_REMOVE=ON] -P MeiosPruneResources.cmake
#
# This is deliberately a separate step rather than something a configure does. Several build trees
# may share one cache, and a configure sees only its own declarations: collecting from inside one
# would delete the trees its siblings are still building against. Reporting is the default; nothing
# is deleted unless MEIOS_PRUNE_REMOVE is on.

cmake_minimum_required(VERSION 3.28)

if(NOT MEIOS_RESOURCE_CACHE_DIR)
    message(FATAL_ERROR
        "MeiosPruneResources: set MEIOS_RESOURCE_CACHE_DIR to the cache directory to examine.")
endif()
if(NOT IS_DIRECTORY "${MEIOS_RESOURCE_CACHE_DIR}")
    message(FATAL_ERROR
        "MeiosPruneResources: '${MEIOS_RESOURCE_CACHE_DIR}' is not a directory.")
endif()

# A build tree is gone once its cache file is: the directory itself can survive a partial delete,
# and an empty shell has no claim on anything.
function(_meios_prune_read_claims file out_owner out_entries)
    set(_owner "")
    set(_entries "")
    file(STRINGS "${file}" _lines)
    foreach(_line IN LISTS _lines)
        if(_line MATCHES "^owner=(.+)$")
            set(_owner "${CMAKE_MATCH_1}")
        elseif(_line MATCHES "^entry=(.+)$")
            list(APPEND _entries "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(_owner AND NOT EXISTS "${_owner}/CMakeCache.txt")
        set(_owner "")
    endif()
    set(${out_owner} "${_owner}" PARENT_SCOPE)
    set(${out_entries} "${_entries}" PARENT_SCOPE)
endfunction()

function(_meios_prune_wanted root out_wanted out_stale_claims)
    set(_wanted "")
    set(_stale "")
    file(GLOB _files "${root}/*.claims")
    foreach(_file IN LISTS _files)
        _meios_prune_read_claims("${_file}" _owner _entries)
        if(_owner)
            list(APPEND _wanted ${_entries})
        else()
            list(APPEND _stale "${_file}")
        endif()
    endforeach()
    set(${out_wanted} "${_wanted}" PARENT_SCOPE)
    set(${out_stale_claims} "${_stale}" PARENT_SCOPE)
endfunction()

function(_meios_prune_describe stamp out)
    set(_what "")
    if(EXISTS "${stamp}")
        file(STRINGS "${stamp}" _lines)
        foreach(_line IN LISTS _lines)
            if(_line MATCHES "^(url|repository|ref)=(.+)$")
                list(APPEND _what "${CMAKE_MATCH_2}")
            endif()
        endforeach()
    endif()
    if(NOT _what)
        set(_what "no stamp — fetch never completed")
    endif()
    string(REPLACE ";" " @ " _text "${_what}")
    set(${out} "${_text}" PARENT_SCOPE)
endfunction()

# Scratch is debris by definition: a fetch that finished renamed its scratch into place or deleted
# it, so anything still carrying a scratch suffix is from a run that did not finish — including
# beside an entry that is otherwise wanted.
function(_meios_prune_unwanted root wanted out)
    set(_dead "")
    file(GLOB _entries LIST_DIRECTORIES TRUE "${root}/*")
    foreach(_entry IN LISTS _entries)
        get_filename_component(_name "${_entry}" NAME)
        if(_name MATCHES "\\.(claims|stamp)$")
            continue()
        endif()
        if(_name MATCHES "\\.(download|extract|clone)$" OR NOT _name IN_LIST wanted)
            list(APPEND _dead "${_entry}")
        endif()
    endforeach()
    set(${out} "${_dead}" PARENT_SCOPE)
endfunction()

function(_meios_prune_collect entries)
    foreach(_entry IN LISTS entries)
        get_filename_component(_name "${_entry}" NAME)
        _meios_prune_describe("${_entry}.stamp" _what)
        message(STATUS "  ${_name}  (${_what})")
        if(MEIOS_PRUNE_REMOVE)
            file(REMOVE_RECURSE "${_entry}")
            file(REMOVE "${_entry}.stamp")
        endif()
    endforeach()
endfunction()

_meios_prune_wanted("${MEIOS_RESOURCE_CACHE_DIR}" _wanted _stale_claims)
_meios_prune_unwanted("${MEIOS_RESOURCE_CACHE_DIR}" "${_wanted}" _dead)

if(_dead)
    message(STATUS "Resources no build tree asks for:")
    _meios_prune_collect("${_dead}")
endif()
if(_stale_claims AND MEIOS_PRUNE_REMOVE)
    file(REMOVE ${_stale_claims})
endif()

if(NOT _dead)
    message(STATUS "Nothing to collect in ${MEIOS_RESOURCE_CACHE_DIR}.")
elseif(NOT MEIOS_PRUNE_REMOVE)
    message(STATUS "Nothing was deleted. Re-run with -DMEIOS_PRUNE_REMOVE=ON to collect these.")
endif()
