include_guard(GLOBAL)

set(MEIOS_RESOURCE_CACHE_DIR "" CACHE PATH
    "Root directory for acquired resource trees (defaults to the build tree).")
set(MEIOS_RESOURCE_TLS_CAINFO "" CACHE FILEPATH
    "CA bundle forwarded to file(DOWNLOAD) as TLS_CAINFO where CMake ships without a trust store.")

# One cache per build tree is the unit the claim and prune lifecycle rests on, so the root is
# anchored at the top of the build: every meios resource declared anywhere in a tree — meios's own
# or a consumer's, at whatever depth — shares that one cache and one owner key.
# A build tree lists what it needs, rather than each tree listing who needs it. The direction is
# what makes collection possible: a claim file is rewritten from scratch by every configure, so
# renaming a resource or deleting its declaration drops the old entry the next time the project
# configures. An entry that recorded its users could only ever grow, and would still name a build
# tree that had long since stopped declaring it.
function(_meios_resource_root out)
    set(_root "${MEIOS_RESOURCE_CACHE_DIR}")
    if(NOT _root)
        set(_root "${CMAKE_BINARY_DIR}/_meios_resources")
    endif()
    set(${out} "${_root}" PARENT_SCOPE)
endfunction()

function(_meios_claims_file out)
    _meios_resource_root(_root)
    string(SHA256 _owner "${CMAKE_BINARY_DIR}")
    string(SUBSTRING "${_owner}" 0 12 _owner)
    set(${out} "${_root}/${_owner}.claims" PARENT_SCOPE)
endfunction()

function(_meios_record_claim entry)
    _meios_claims_file(_claims)
    if(NOT EXISTS "${_claims}")
        file(WRITE "${_claims}" "owner=${CMAKE_BINARY_DIR}\n")
    endif()
    file(APPEND "${_claims}" "entry=${entry}\n")
endfunction()

# Dropped here, at include time, rather than when the first resource is declared: a project that
# deleted its last declaration never reaches a declaration to reset it, and would go on claiming
# what it no longer uses. The directory is not created — a consumer that declares nothing leaves
# no trace.
_meios_claims_file(_meios_claims)
file(REMOVE "${_meios_claims}")
unset(_meios_claims)
