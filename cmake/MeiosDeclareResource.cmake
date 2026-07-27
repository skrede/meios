include_guard(GLOBAL)

# A resource is a named directory tree of DATA (parsed, never compiled, never linked), however it was
# acquired; every acquisition mode resolves to a directory consumed as source_kind::directory.
# Acquisition (meios_declare_resource) is separated from placement (meios_target_deploy_resources)
# because the two vary independently: one tree may be deployed beside several targets, and a target's
# layout is decided in its own directory, not where the tree came from.

include("${CMAKE_CURRENT_LIST_DIR}/MeiosResourceCache.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/MeiosResourcePaths.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/MeiosResourceLfs.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/MeiosResourceArgs.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/MeiosResourceGit.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/MeiosResourceArchive.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/MeiosResourceRegistry.cmake")
# Two call sites name this module by hand — the top-level listfile for the in-tree path and the
# package config template for the installed path — so a consumer that includes only this module
# must still reach every public resource function.
include("${CMAKE_CURRENT_LIST_DIR}/MeiosDeployResources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/MeiosFlattenResource.cmake")

# The tree lives at a path derived from every argument that determines its bytes, rather than at
# one named after the resource. A key assembled by hand can forget an argument and hand back a
# tree fetched under different ones; a key that IS the argument list cannot. It also lets two
# build trees pin different revisions of the same resource through one shared cache — sharing a
# single <name> directory, they would overwrite each other's tree on every configure.
function(_meios_resource_cache_key url hash strip repository ref sparse out_descriptor out_digest)
    if(url)
        set(_strip 0)
        if(strip)
            set(_strip 1)
        endif()
        set(_descriptor "mode=url\nurl=${url}\nhash=${hash}\nstrip=${_strip}")
    else()
        string(CONCAT _descriptor "mode=git\nrepository=${repository}\n"
                                  "ref=${ref}\nsparse=${sparse}")
    endif()
    string(SHA256 _digest "${_descriptor}")
    string(SUBSTRING "${_digest}" 0 12 _digest)
    set(${out_descriptor} "${_descriptor}" PARENT_SCOPE)
    set(${out_digest} "${_digest}" PARENT_SCOPE)
endfunction()

# An unpinned URL names where the bytes came from but nothing about what was served, so a tree
# fetched under one is never reused however intact it looks.
function(_meios_resource_cached url hash tree stamp out)
    set(_cached FALSE)
    if(EXISTS "${stamp}" AND IS_DIRECTORY "${tree}" AND NOT (url AND NOT hash))
        set(_cached TRUE)
    endif()
    set(${out} "${_cached}" PARENT_SCOPE)
endfunction()

# Pre-populated override: skip all fetching, but still scan — a hand-placed tree can be unsmudged.
function(_meios_declare_placed name source_dir subdir out)
    if(NOT IS_DIRECTORY "${source_dir}")
        message(FATAL_ERROR
            "meios_declare_resource(${name}): SOURCE_DIR '${source_dir}' is not a directory.")
    endif()
    set(_tree "${source_dir}")
    if(subdir)
        _meios_resource_subdir("${name}" "${_tree}" "${subdir}" _tree)
    endif()
    _meios_scan_lfs_pointers("${name}" "${_tree}")
    set(${out} "${_tree}" PARENT_SCOPE)
endfunction()

function(_meios_declare_acquire name url hash strip repository tag sparse dest)
    if(url)
        _meios_url_acquire("${name}" "${url}" "${hash}" "${strip}" "${dest}")
    else()
        # Escape hatch for LFS, submodules, and private auth, and the only mode that can fetch part
        # of a tree rather than all of it.
        _meios_git_acquire("${name}" "${repository}" "${tag}" "${sparse}" "${dest}")
    endif()
endfunction()

function(_meios_declare_stamp name tree stamp descriptor)
    _meios_scan_lfs_pointers("${name}" "${tree}")
    # Written last, so an interrupted fetch leaves a tree with no stamp rather than one that
    # passes for complete. It also carries the descriptor, which is the only readable account
    # of what a digest-named directory holds.
    file(WRITE "${stamp}" "${descriptor}\n")
endfunction()

function(_meios_declare_fetch name url hash strip repository tag sparse subdir root out)
    _meios_resource_cache_key("${url}" "${hash}" "${strip}" "${repository}" "${tag}" "${sparse}"
                              _descriptor _digest)
    set(_source_dir "${root}/${name}-${_digest}")
    set(_stamp "${_source_dir}.stamp")
    _meios_resource_cached("${url}" "${hash}" "${_source_dir}" "${_stamp}" _cached)
    if(NOT _cached)
        _meios_declare_acquire("${name}" "${url}" "${hash}" "${strip}" "${repository}"
                               "${tag}" "${sparse}" "${_source_dir}")
    endif()
    set(_tree "${_source_dir}")
    if(subdir)
        _meios_resource_subdir("${name}" "${_tree}" "${subdir}" _tree)
    endif()
    if(NOT _cached)
        _meios_declare_stamp("${name}" "${_tree}" "${_stamp}" "${_descriptor}")
    endif()
    get_filename_component(_entry "${_source_dir}" NAME)
    _meios_record_claim("${_entry}")
    set(${out} "${_tree}" PARENT_SCOPE)
endfunction()

function(_meios_declare_tree name source_dir url hash strip repository tag sparse subdir out)
    _meios_resource_root(_root)
    file(MAKE_DIRECTORY "${_root}")
    if(source_dir)
        _meios_declare_placed("${name}" "${source_dir}" "${subdir}" _tree)
    else()
        _meios_declare_fetch("${name}" "${url}" "${hash}" "${strip}" "${repository}" "${tag}"
                             "${sparse}" "${subdir}" "${_root}" _tree)
    endif()
    set(${out} "${_tree}" PARENT_SCOPE)
endfunction()

function(meios_declare_resource)
    set(options       STRIP_TOP_LEVEL)
    set(one_value     NAME URL HASH GIT_REPOSITORY GIT_TAG GITHUB REF SOURCE_DIR SUBDIR OUT_DIR)
    set(multi_value  SPARSE_PATHS)
    cmake_parse_arguments(ARG "${options}" "${one_value}" "${multi_value}" ${ARGN})

    _meios_declare_validate("${ARG_NAME}" "${ARG_UNPARSED_ARGUMENTS}" "${ARG_GITHUB}" "${ARG_REF}"
                            "${ARG_SPARSE_PATHS}" "${ARG_SOURCE_DIR}" "${ARG_URL}" "${ARG_HASH}")
    _meios_declare_override("${ARG_NAME}" ARG_SOURCE_DIR ARG_URL ARG_GIT_REPOSITORY ARG_GITHUB)
    _meios_declare_normalize("${ARG_NAME}" "${ARG_SOURCE_DIR}" "${ARG_URL}" "${ARG_GIT_REPOSITORY}"
                             "${ARG_GITHUB}" "${ARG_REF}" "${ARG_SPARSE_PATHS}" ARG_URL
                             ARG_GIT_REPOSITORY ARG_GIT_TAG ARG_GITHUB ARG_STRIP_TOP_LEVEL)
    _meios_declare_tree("${ARG_NAME}" "${ARG_SOURCE_DIR}" "${ARG_URL}" "${ARG_HASH}"
                        "${ARG_STRIP_TOP_LEVEL}" "${ARG_GIT_REPOSITORY}" "${ARG_GIT_TAG}"
                        "${ARG_SPARSE_PATHS}" "${ARG_SUBDIR}" _tree)

    _meios_register_resource("${ARG_NAME}" "${_tree}")
    if(ARG_OUT_DIR)
        set(${ARG_OUT_DIR} "${_tree}" PARENT_SCOPE)
    endif()
endfunction()
