include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/MeiosResourcePaths.cmake")

function(_meios_declare_check_name name)
    if(NOT name)
        message(FATAL_ERROR "meios_declare_resource: NAME is required.")
    endif()
    # NAME is a path component of the cache entry, and the entry is removed recursively before it is
    # written, so a name carrying a separator or a parent component aims both the write and the
    # delete outside the cache root. It also names a global property, which the same set admits.
    if(NOT name MATCHES "^[A-Za-z0-9_][A-Za-z0-9_.+-]*$")
        message(FATAL_ERROR
            "meios_declare_resource: NAME must be a plain identifier — letters, digits, '_', '.', "
            "'+' or '-', starting with a letter, digit or '_'; got '${name}'.")
    endif()
endfunction()

function(_meios_declare_check_github name github ref)
    if(ref AND NOT github)
        message(FATAL_ERROR
            "meios_declare_resource(${name}): REF names the revision for GITHUB; use GIT_TAG "
            "with GIT_REPOSITORY, or put the revision in the URL.")
    endif()
    if(NOT github)
        return()
    endif()
    if(NOT github MATCHES "^[A-Za-z0-9._-]+/[A-Za-z0-9._-]+$")
        message(FATAL_ERROR
            "meios_declare_resource(${name}): GITHUB must be <owner>/<repository>; "
            "got '${github}'.")
    endif()
    if(NOT ref)
        message(FATAL_ERROR
            "meios_declare_resource(${name}): GITHUB requires REF (a tag, branch, or commit).")
    endif()
endfunction()

function(_meios_declare_check_sparse name sparse source_dir url hash)
    if(NOT sparse)
        return()
    endif()
    if(source_dir OR url)
        message(FATAL_ERROR
            "meios_declare_resource(${name}): SPARSE_PATHS slices a clone; an archive is "
            "served whole and a pre-placed tree is already on disk. Use GIT_REPOSITORY or GITHUB.")
    endif()
    if(hash)
        message(FATAL_ERROR
            "meios_declare_resource(${name}): HASH pins an archive's bytes; a sliced clone "
            "has none. Pin the revision with a commit REF or GIT_TAG instead.")
    endif()
    _meios_check_contained_paths("meios_declare_resource(${name})" SPARSE_PATHS "${sparse}")
endfunction()

function(_meios_declare_validate name unparsed github ref sparse source_dir url hash)
    _meios_declare_check_name("${name}")
    if(unparsed)
        message(FATAL_ERROR
            "meios_declare_resource(${name}): unknown args: ${unparsed}")
    endif()
    _meios_declare_check_github("${name}" "${github}" "${ref}")
    _meios_declare_check_sparse("${name}" "${sparse}" "${source_dir}" "${url}" "${hash}")
endfunction()

# Declared per resource so an offline or air-gapped configure can redirect any mode at a
# pre-placed tree without editing the listfile that declares it.
function(_meios_declare_override name out_source_dir out_url out_repository out_github)
    set(MEIOS_RESOURCE_${name}_SOURCE_DIR "" CACHE PATH
        "Pre-placed tree overriding acquisition of the '${name}' resource.")
    if(NOT MEIOS_RESOURCE_${name}_SOURCE_DIR)
        return()
    endif()
    set(${out_source_dir} "${MEIOS_RESOURCE_${name}_SOURCE_DIR}" PARENT_SCOPE)
    set(${out_url} "" PARENT_SCOPE)
    set(${out_repository} "" PARENT_SCOPE)
    set(${out_github} "" PARENT_SCOPE)
endfunction()

# Sugar over URL mode: GitHub serves <ref>.tar.gz for a tag, branch, or commit alike, and always
# wraps the tree in a <repository>-<ref> directory, so the strip is implied rather than asked for.
# SPARSE_PATHS turns the same declaration into a clone instead: GitHub has no endpoint that
# serves part of a tree, so slicing is only reachable over the git protocol.
function(_meios_declare_normalize name source_dir url repository github ref sparse
                                  out_url out_repository out_tag out_github out_strip)
    set(_modes 0)
    foreach(_m source_dir url repository github)
        if(${_m})
            math(EXPR _modes "${_modes}+1")
        endif()
    endforeach()
    if(NOT _modes EQUAL 1)
        message(FATAL_ERROR
            "meios_declare_resource(${name}): specify exactly one of "
            "SOURCE_DIR / URL / GIT_REPOSITORY / GITHUB.")
    endif()
    if(github AND sparse)
        set(${out_repository} "https://github.com/${github}.git" PARENT_SCOPE)
        set(${out_tag} "${ref}" PARENT_SCOPE)
        set(${out_github} "" PARENT_SCOPE)
    elseif(github)
        set(${out_url} "https://github.com/${github}/archive/${ref}.tar.gz" PARENT_SCOPE)
        set(${out_strip} TRUE PARENT_SCOPE)
    endif()
endfunction()
