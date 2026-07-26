include_guard(GLOBAL)

# A resource is a named directory tree of DATA (parsed, never compiled, never linked), however it was
# acquired; every acquisition mode resolves to a directory consumed as source_kind::directory.
# Acquisition (meios_declare_resource) is separated from placement (meios_target_deploy_resources)
# because the two vary independently: one tree may be deployed beside several targets, and a target's
# layout is decided in its own directory, not where the tree came from.
#
# The registry is a GLOBAL property rather than a PARENT_SCOPE variable so a resource declared in the
# top-level listfile is visible in a sibling subdirectory, which a PARENT_SCOPE variable never reaches.

set(MEIOS_RESOURCE_CACHE_DIR "" CACHE PATH
    "Root directory for acquired resource trees (defaults to the build tree).")
set(MEIOS_RESOURCE_TLS_CAINFO "" CACHE FILEPATH
    "CA bundle forwarded to file(DOWNLOAD) as TLS_CAINFO where CMake ships without a trust store.")

# A git-lfs pointer is UTF-8 text, at most 1024 bytes, whose first line is the spec version prefix
# (https://github.com/git-lfs/git-lfs/blob/main/docs/spec.md). Real meshes are larger or begin with
# their own magic/markup, so the size bound plus the prefix is exact with no false positives.
function(_meios_is_lfs_pointer path out)
    set(${out} FALSE PARENT_SCOPE)
    file(SIZE "${path}" _sz)
    if(_sz GREATER 0 AND _sz LESS_EQUAL 1024)
        file(READ "${path}" _head LIMIT 64)
        if(_head MATCHES "^version https://git-lfs\\.github\\.com/spec/")
            set(${out} TRUE PARENT_SCOPE)
        endif()
    endif()
endfunction()

# Fail loudly on any unsmudged pointer: a pointer resolves as a valid path but carries no geometry,
# so a silent success would ship a robot with stub meshes.
function(_meios_scan_lfs_pointers name dir)
    file(GLOB_RECURSE _meshes
         "${dir}/*.stl" "${dir}/*.dae" "${dir}/*.obj" "${dir}/*.ply" "${dir}/*.glb")
    set(_ptrs "")
    foreach(_m IN LISTS _meshes)
        _meios_is_lfs_pointer("${_m}" _p)
        if(_p)
            list(APPEND _ptrs "${_m}")
        endif()
    endforeach()
    if(_ptrs)
        string(REPLACE ";" "\n    " _plist "${_ptrs}")
        message(FATAL_ERROR
            "meios_declare_resource(${name}): unsmudged Git-LFS pointers found — these meshes are "
            "text stubs, not geometry:\n    ${_plist}\n"
            "Fetch a properly-smudged archive, or use GIT_REPOSITORY mode with Git-LFS smudging enabled.")
    endif()
endfunction()

# SUBDIR is caller-supplied text joined onto an acquired tree, so it is checked the way
# directory_source checks a package-relative path: a traversal must not silently widen the tree.
function(_meios_resource_subdir name root rel out)
    file(REAL_PATH "${root}" _base)
    file(REAL_PATH "${root}/${rel}" _cand)
    file(RELATIVE_PATH _probe "${_base}" "${_cand}")
    if(_probe MATCHES "^\\.\\.")
        message(FATAL_ERROR
            "meios_declare_resource(${name}): SUBDIR '${rel}' escapes the acquired tree.")
    endif()
    if(NOT IS_DIRECTORY "${_cand}")
        message(FATAL_ERROR
            "meios_declare_resource(${name}): SUBDIR '${rel}' is not a directory under the "
            "acquired tree:\n  ${_cand}")
    endif()
    set(${out} "${_cand}" PARENT_SCOPE)
endfunction()

function(_meios_check_sparse_paths name paths)
    foreach(_p IN LISTS paths)
        if(IS_ABSOLUTE "${_p}" OR _p MATCHES "(^|/)\\.\\.(/|$)" OR _p MATCHES "^-")
            message(FATAL_ERROR
                "meios_declare_resource(${name}): SPARSE_PATHS entry '${_p}' must be a relative "
                "path inside the repository, with no '..' component and no leading '-'.")
        endif()
    endforeach()
endfunction()

function(_meios_run_git name dir what)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${dir}" ${ARGN}
        RESULT_VARIABLE _rc
        ERROR_VARIABLE  _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "meios_declare_resource(${name}): git ${what} failed: ${_err}")
    endif()
endfunction()

# Cone mode reports success for a pattern that matches nothing, so a mistyped path yields a tree
# silently missing a whole directory rather than an error — the checkout is only as trustworthy as
# this check.
function(_meios_check_sparse_result name dir paths)
    set(_missing "")
    foreach(_p IN LISTS paths)
        if(NOT EXISTS "${dir}/${_p}")
            list(APPEND _missing "${_p}")
        endif()
    endforeach()
    if(_missing)
        string(REPLACE ";" "\n    " _list "${_missing}")
        message(FATAL_ERROR
            "meios_declare_resource(${name}): SPARSE_PATHS selected nothing for:\n    ${_list}\n"
            "Cone-mode sparse checkout succeeds on a pattern that matches no path, so these are "
            "typos or paths absent at this revision.")
    endif()
endfunction()

# Publishes through a scratch clone rather than cloning straight into place: the rename is the
# atomic step, and it is also where .git is dropped, so an acquired tree is data alone and matches
# what the archive path produces. Large-file objects are left as their pointers and submodules
# uninitialized on purpose — the pointer scan fails loudly rather than shipping stub geometry.
function(_meios_git_acquire name repository ref paths dest)
    find_package(Git REQUIRED)
    if(paths AND GIT_VERSION_STRING VERSION_LESS 2.28)
        message(FATAL_ERROR
            "meios_declare_resource(${name}): SPARSE_PATHS needs 'git sparse-checkout set --cone', "
            "which requires Git 2.28 or newer; found ${GIT_VERSION_STRING}.")
    endif()

    set(_branch_arg "")
    if(ref)
        set(_branch_arg --branch "${ref}")
    endif()
    set(_sparse_arg "")
    if(paths)
        set(_sparse_arg --filter=blob:none --no-checkout)
    endif()

    set(_scratch "${dest}.clone")
    file(REMOVE_RECURSE "${_scratch}")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" clone --depth 1 ${_branch_arg} ${_sparse_arg}
                -- "${repository}" "${_scratch}"
        RESULT_VARIABLE _rc
        ERROR_VARIABLE  _err)
    if(NOT _rc EQUAL 0)
        file(REMOVE_RECURSE "${_scratch}")
        message(FATAL_ERROR "meios_declare_resource(${name}): git clone failed: ${_err}")
    endif()

    if(paths)
        _meios_run_git("${name}" "${_scratch}" "sparse-checkout" sparse-checkout set --cone ${paths})
        _meios_run_git("${name}" "${_scratch}" "checkout" checkout)
        _meios_check_sparse_result("${name}" "${_scratch}" "${paths}")
    endif()

    file(REMOVE_RECURSE "${_scratch}/.git")
    file(REMOVE_RECURSE "${dest}")
    file(RENAME "${_scratch}" "${dest}")
endfunction()

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

function(_meios_register_resource name dir)
    get_property(_known GLOBAL PROPERTY MEIOS_RESOURCES)
    if("${name}" IN_LIST _known)
        get_property(_prev GLOBAL PROPERTY MEIOS_RESOURCE_${name}_DIR)
        if(NOT _prev STREQUAL "${dir}")
            message(FATAL_ERROR
                "meios_declare_resource(${name}): already declared with a different tree.\n"
                "  first: ${_prev}\n  now:   ${dir}")
        endif()
        return()
    endif()
    set_property(GLOBAL APPEND PROPERTY MEIOS_RESOURCES "${name}")
    set_property(GLOBAL PROPERTY MEIOS_RESOURCE_${name}_DIR "${dir}")
endfunction()

# Query a declared resource from any directory scope.
function(meios_resource_dir name out)
    get_property(_known GLOBAL PROPERTY MEIOS_RESOURCES)
    if(NOT "${name}" IN_LIST _known)
        string(REPLACE ";" ", " _list "${_known}")
        message(FATAL_ERROR
            "meios_resource_dir: no resource named '${name}'. Declared: ${_list}")
    endif()
    get_property(_dir GLOBAL PROPERTY MEIOS_RESOURCE_${name}_DIR)
    set(${out} "${_dir}" PARENT_SCOPE)
endfunction()

function(meios_declare_resource)
    set(options       STRIP_TOP_LEVEL)
    set(one_value     NAME URL HASH GIT_REPOSITORY GIT_TAG GITHUB REF SOURCE_DIR SUBDIR OUT_DIR)
    set(multi_value  SPARSE_PATHS)
    cmake_parse_arguments(ARG "${options}" "${one_value}" "${multi_value}" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "meios_declare_resource: NAME is required.")
    endif()
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "meios_declare_resource(${ARG_NAME}): unknown args: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(ARG_REF AND NOT ARG_GITHUB)
        message(FATAL_ERROR
            "meios_declare_resource(${ARG_NAME}): REF names the revision for GITHUB; use GIT_TAG "
            "with GIT_REPOSITORY, or put the revision in the URL.")
    endif()
    if(ARG_GITHUB)
        if(NOT ARG_GITHUB MATCHES "^[A-Za-z0-9._-]+/[A-Za-z0-9._-]+$")
            message(FATAL_ERROR
                "meios_declare_resource(${ARG_NAME}): GITHUB must be <owner>/<repository>; "
                "got '${ARG_GITHUB}'.")
        endif()
        if(NOT ARG_REF)
            message(FATAL_ERROR
                "meios_declare_resource(${ARG_NAME}): GITHUB requires REF (a tag, branch, or commit).")
        endif()
    endif()
    if(ARG_SPARSE_PATHS)
        if(ARG_SOURCE_DIR OR ARG_URL)
            message(FATAL_ERROR
                "meios_declare_resource(${ARG_NAME}): SPARSE_PATHS slices a clone; an archive is "
                "served whole and a pre-placed tree is already on disk. Use GIT_REPOSITORY or GITHUB.")
        endif()
        if(ARG_HASH)
            message(FATAL_ERROR
                "meios_declare_resource(${ARG_NAME}): HASH pins an archive's bytes; a sliced clone "
                "has none. Pin the revision with a commit REF or GIT_TAG instead.")
        endif()
        _meios_check_sparse_paths("${ARG_NAME}" "${ARG_SPARSE_PATHS}")
    endif()

    # Declared per resource so an offline or air-gapped configure can redirect any mode at a
    # pre-placed tree without editing the listfile that declares it.
    set(MEIOS_RESOURCE_${ARG_NAME}_SOURCE_DIR "" CACHE PATH
        "Pre-placed tree overriding acquisition of the '${ARG_NAME}' resource.")
    if(MEIOS_RESOURCE_${ARG_NAME}_SOURCE_DIR)
        set(ARG_SOURCE_DIR "${MEIOS_RESOURCE_${ARG_NAME}_SOURCE_DIR}")
        set(ARG_URL "")
        set(ARG_GIT_REPOSITORY "")
        set(ARG_GITHUB "")
    endif()

    set(_modes 0)
    foreach(_m SOURCE_DIR URL GIT_REPOSITORY GITHUB)
        if(ARG_${_m})
            math(EXPR _modes "${_modes}+1")
        endif()
    endforeach()
    if(NOT _modes EQUAL 1)
        message(FATAL_ERROR
            "meios_declare_resource(${ARG_NAME}): specify exactly one of "
            "SOURCE_DIR / URL / GIT_REPOSITORY / GITHUB.")
    endif()

    # Sugar over URL mode: GitHub serves <ref>.tar.gz for a tag, branch, or commit alike, and always
    # wraps the tree in a <repository>-<ref> directory, so the strip is implied rather than asked for.
    # SPARSE_PATHS turns the same declaration into a clone instead: GitHub has no endpoint that
    # serves part of a tree, so slicing is only reachable over the git protocol.
    if(ARG_GITHUB AND ARG_SPARSE_PATHS)
        set(ARG_GIT_REPOSITORY "https://github.com/${ARG_GITHUB}.git")
        set(ARG_GIT_TAG "${ARG_REF}")
        set(ARG_GITHUB "")
    elseif(ARG_GITHUB)
        set(ARG_URL "https://github.com/${ARG_GITHUB}/archive/${ARG_REF}.tar.gz")
        set(ARG_STRIP_TOP_LEVEL TRUE)
    endif()

    _meios_resource_root(_root)
    file(MAKE_DIRECTORY "${_root}")

    # Pre-populated override: skip all fetching, but still scan — a hand-placed tree can be unsmudged.
    if(ARG_SOURCE_DIR)
        if(NOT IS_DIRECTORY "${ARG_SOURCE_DIR}")
            message(FATAL_ERROR
                "meios_declare_resource(${ARG_NAME}): SOURCE_DIR '${ARG_SOURCE_DIR}' is not a directory.")
        endif()
        set(_tree "${ARG_SOURCE_DIR}")
        if(ARG_SUBDIR)
            _meios_resource_subdir("${ARG_NAME}" "${_tree}" "${ARG_SUBDIR}" _tree)
        endif()
        _meios_scan_lfs_pointers("${ARG_NAME}" "${_tree}")
        _meios_register_resource("${ARG_NAME}" "${_tree}")
        if(ARG_OUT_DIR)
            set(${ARG_OUT_DIR} "${_tree}" PARENT_SCOPE)
        endif()
        return()
    endif()

    # The tree lives at a path derived from every argument that determines its bytes, rather than at
    # one named after the resource. A key assembled by hand can forget an argument and hand back a
    # tree fetched under different ones; a key that IS the argument list cannot. It also lets two
    # build trees pin different revisions of the same resource through one shared cache — sharing a
    # single <name> directory, they would overwrite each other's tree on every configure.
    if(ARG_URL)
        set(_strip 0)
        if(ARG_STRIP_TOP_LEVEL)
            set(_strip 1)
        endif()
        set(_descriptor "mode=url\nurl=${ARG_URL}\nhash=${ARG_HASH}\nstrip=${_strip}")
    else()
        string(CONCAT _descriptor "mode=git\nrepository=${ARG_GIT_REPOSITORY}\n"
                                  "ref=${ARG_GIT_TAG}\nsparse=${ARG_SPARSE_PATHS}")
    endif()
    string(SHA256 _digest "${_descriptor}")
    string(SUBSTRING "${_digest}" 0 12 _digest)
    set(_source_dir "${_root}/${ARG_NAME}-${_digest}")
    set(_stamp "${_source_dir}.stamp")

    # An unpinned URL names where the bytes came from but nothing about what was served, so a tree
    # fetched under one is never reused however intact it looks.
    set(_cached FALSE)
    if(EXISTS "${_stamp}" AND IS_DIRECTORY "${_source_dir}" AND NOT (ARG_URL AND NOT ARG_HASH))
        set(_cached TRUE)
    endif()

    if(NOT _cached AND ARG_URL)
        set(_hash_arg "")
        if(ARG_HASH)
            if(NOT ARG_HASH MATCHES "^(SHA256|SHA512|SHA384|SHA224|SHA1)=[0-9a-fA-F]+$")
                message(FATAL_ERROR
                    "meios_declare_resource(${ARG_NAME}): HASH must be <ALGO>=<hex> "
                    "(e.g. SHA256=ab…); got '${ARG_HASH}'.")
            endif()
            set(_hash_arg EXPECTED_HASH "${ARG_HASH}")
        endif()

        set(_tls_ca_arg "")
        if(MEIOS_RESOURCE_TLS_CAINFO)
            set(_tls_ca_arg TLS_CAINFO "${MEIOS_RESOURCE_TLS_CAINFO}")
        endif()

        set(_archive "${_source_dir}.download")
        # TLS_VERIFY defaults OFF below CMake 3.31; the project floor is 3.28, so it is passed
        # explicitly on every call and no public knob disables it.
        file(DOWNLOAD "${ARG_URL}" "${_archive}"
             ${_hash_arg}
             TLS_VERIFY ON
             ${_tls_ca_arg}
             SHOW_PROGRESS
             INACTIVITY_TIMEOUT 60
             STATUS _dl_status
             LOG     _dl_log)
        list(GET _dl_status 0 _dl_code)
        list(GET _dl_status 1 _dl_msg)
        if(NOT _dl_code EQUAL 0)
            # A partial or hash-mismatched file must never survive to look like a valid cache entry.
            file(REMOVE "${_archive}")
            message(FATAL_ERROR
                "meios_declare_resource(${ARG_NAME}): download failed "
                "(code ${_dl_code}: ${_dl_msg})\n  url:  ${ARG_URL}\n  log:\n${_dl_log}")
        endif()

        # Warned after the download rather than before it so the message can carry the hash that
        # silences it: the only thing the caller is missing is a value they would otherwise have to
        # compute by hand.
        if(NOT ARG_HASH)
            file(SHA256 "${_archive}" _computed)
            message(WARNING
                "meios_declare_resource(${ARG_NAME}): fetched WITHOUT an integrity hash. The download "
                "is trusted purely on TLS and server honesty, a swapped artifact is accepted silently, "
                "and nothing is cached, so every configure re-downloads. Pin it by adding:\n"
                "    HASH SHA256=${_computed}")
        endif()

        set(_extract_tmp "${_source_dir}.extract")
        file(REMOVE_RECURSE "${_extract_tmp}")
        file(ARCHIVE_EXTRACT INPUT "${_archive}" DESTINATION "${_extract_tmp}")

        set(_extracted "${_extract_tmp}")
        if(ARG_STRIP_TOP_LEVEL)
            file(GLOB _top LIST_DIRECTORIES TRUE "${_extract_tmp}/*")
            list(LENGTH _top _n)
            if(_n EQUAL 1)
                list(GET _top 0 _top0)
                if(IS_DIRECTORY "${_top0}")
                    set(_extracted "${_top0}")
                endif()
            endif()
        endif()

        # Publish atomically: extraction happened in a scratch dir we own, so a crash mid-extract
        # cannot leave a half-tree that looks cached. Extraction under an owned scratch dir also
        # confines the blast radius of a hostile '../' entry (CMake rejects traversal only in 4.3+).
        file(REMOVE_RECURSE "${_source_dir}")
        file(RENAME "${_extracted}" "${_source_dir}")
        file(REMOVE_RECURSE "${_extract_tmp}")
        # The extracted tree is the cache; keeping the archive beside it stores every resource
        # twice for the sake of an extract that only happens if the tree is deleted by hand.
        file(REMOVE "${_archive}")
    elseif(NOT _cached)
        # Escape hatch for LFS, submodules, and private auth, and the only mode that can fetch part
        # of a tree rather than all of it.
        _meios_git_acquire("${ARG_NAME}" "${ARG_GIT_REPOSITORY}" "${ARG_GIT_TAG}"
                           "${ARG_SPARSE_PATHS}" "${_source_dir}")
    endif()

    set(_tree "${_source_dir}")
    if(ARG_SUBDIR)
        _meios_resource_subdir("${ARG_NAME}" "${_tree}" "${ARG_SUBDIR}" _tree)
    endif()

    if(NOT _cached)
        _meios_scan_lfs_pointers("${ARG_NAME}" "${_tree}")
        # Written last, so an interrupted fetch leaves a tree with no stamp rather than one that
        # passes for complete. It also carries the descriptor, which is the only readable account
        # of what a digest-named directory holds.
        file(WRITE "${_stamp}" "${_descriptor}\n")
    endif()
    get_filename_component(_entry "${_source_dir}" NAME)
    _meios_record_claim("${_entry}")

    _meios_register_resource("${ARG_NAME}" "${_tree}")
    if(ARG_OUT_DIR)
        set(${ARG_OUT_DIR} "${_tree}" PARENT_SCOPE)
    endif()
endfunction()

# Place declared resources where a built target expects them. SUBDIR is relative to the target's
# runtime directory; several resources may share one SUBDIR, which is how sibling description
# packages end up under a single package-root directory.
function(meios_target_deploy_resources target)
    set(options       INSTALL_RUNTIME_RELATIVE)
    set(one_value     SUBDIR INSTALL_DESTINATION INSTALL_COMPONENT)
    set(multi_value   RESOURCES PACKAGES)
    cmake_parse_arguments(ARG "${options}" "${one_value}" "${multi_value}" ${ARGN})

    if(NOT TARGET ${target})
        message(FATAL_ERROR
            "meios_target_deploy_resources: '${target}' is not a target.")
    endif()
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "meios_target_deploy_resources(${target}): unknown args: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT ARG_RESOURCES)
        message(FATAL_ERROR
            "meios_target_deploy_resources(${target}): RESOURCES is required.")
    endif()
    if(ARG_INSTALL_RUNTIME_RELATIVE AND ARG_INSTALL_DESTINATION)
        message(FATAL_ERROR
            "meios_target_deploy_resources(${target}): INSTALL_RUNTIME_RELATIVE and "
            "INSTALL_DESTINATION both name an install location; pass one.")
    endif()

    # Mirrors SUBDIR under the runtime destination so a program that finds its resources relative to
    # its own executable keeps working once installed. Installing the tree to a conventional data
    # directory instead would install successfully and still break such a program at runtime, and
    # only after install — the build tree resolves either way.
    if(ARG_INSTALL_RUNTIME_RELATIVE)
        include(GNUInstallDirs)
        set(ARG_INSTALL_DESTINATION "${CMAKE_INSTALL_BINDIR}")
        if(ARG_SUBDIR)
            set(ARG_INSTALL_DESTINATION "${ARG_INSTALL_DESTINATION}/${ARG_SUBDIR}")
        endif()
    endif()

    if(ARG_INSTALL_COMPONENT AND NOT ARG_INSTALL_DESTINATION)
        message(FATAL_ERROR
            "meios_target_deploy_resources(${target}): INSTALL_COMPONENT requires "
            "INSTALL_DESTINATION or INSTALL_RUNTIME_RELATIVE.")
    endif()

    # PACKAGES names entries to copy out of ONE tree, so more than one source tree would leave the
    # name ambiguous. Filtering is a property of what gets shipped, not of the acquired tree, which
    # is why it lives here rather than on the declaration.
    list(LENGTH ARG_RESOURCES _resource_count)
    if(ARG_PACKAGES AND NOT _resource_count EQUAL 1)
        message(FATAL_ERROR
            "meios_target_deploy_resources(${target}): PACKAGES selects entries from a single "
            "resource tree; pass exactly one RESOURCES name (got ${_resource_count}).")
    endif()

    set(_dest "$<TARGET_FILE_DIR:${target}>")
    if(ARG_SUBDIR)
        if(IS_ABSOLUTE "${ARG_SUBDIR}")
            message(FATAL_ERROR
                "meios_target_deploy_resources(${target}): SUBDIR must be relative; got '${ARG_SUBDIR}'.")
        endif()
        set(_dest "${_dest}/${ARG_SUBDIR}")
    endif()

    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _slot "${ARG_SUBDIR}")

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/meios_deploy")

    foreach(_name IN LISTS ARG_RESOURCES)
        meios_resource_dir("${_name}" _dir)

        # Each entry keeps its own directory name at the destination, because package://<name>/…
        # resolves to <package root>/<name>/…: copying a selected package's *contents* into the
        # package root would strip the very name the reference is looked up under.
        # "." stands for the tree itself. An empty string cannot serve as the sentinel: a list whose
        # only element is "" is an empty list to foreach(IN LISTS), which would silently deploy
        # nothing at all whenever PACKAGES was absent.
        if(ARG_PACKAGES)
            set(_selection ${ARG_PACKAGES})
            foreach(_pkg IN LISTS _selection)
                if(NOT IS_DIRECTORY "${_dir}/${_pkg}")
                    message(FATAL_ERROR
                        "meios_target_deploy_resources(${target}): resource '${_name}' has no "
                        "entry '${_pkg}' to select:\n  ${_dir}/${_pkg}")
                endif()
            endforeach()
        else()
            set(_selection ".")
        endif()

        foreach(_tree IN LISTS _selection)
            if(_tree STREQUAL ".")
                set(_src "${_dir}")
                set(_dst "${_dest}")
                set(_label "${_name}")
            else()
                set(_src "${_dir}/${_tree}")
                set(_dst "${_dest}/${_tree}")
                set(_label "${_name}.${_tree}")
            endif()

            # Driven by an OUTPUT rule keyed on the tree's contents rather than a POST_BUILD
            # command: POST_BUILD runs only when the target itself relinks, so editing a resource
            # without touching a source file would leave a stale tree deployed with no sign
            # anything was wrong. copy_directory_if_different keeps an unchanged tree from
            # restamping every file.
            file(GLOB_RECURSE _files CONFIGURE_DEPENDS "${_src}/*")
            # Keyed on $<CONFIG> because the destination is: under a multi-config generator each
            # configuration has its own runtime directory, and one shared stamp would let the first
            # configuration built mark the rest up to date and leave them without the tree. The
            # configuration goes in the file name, not a directory, so the holding directory can be
            # created once above — `cmake -E touch` does not create parents, and a generator
            # expression cannot be resolved at configure time to create them.
            set(_stamp
                "${CMAKE_CURRENT_BINARY_DIR}/meios_deploy/${target}.${_slot}.${_label}.$<CONFIG>.stamp")
            add_custom_command(
                OUTPUT  "${_stamp}"
                COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${_src}" "${_dst}"
                COMMAND ${CMAKE_COMMAND} -E touch "${_stamp}"
                DEPENDS ${_files}
                COMMENT "Deploying resource '${_label}' to ${_dst}"
                VERBATIM)
            string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _label_slot "${_label}")
            add_custom_target(${target}_deploy_${_slot}_${_label_slot} DEPENDS "${_stamp}")
            add_dependencies(${target} ${target}_deploy_${_slot}_${_label_slot})

            if(ARG_INSTALL_DESTINATION)
                set(_component_arg "")
                if(ARG_INSTALL_COMPONENT)
                    set(_component_arg COMPONENT "${ARG_INSTALL_COMPONENT}")
                endif()
                set(_install_dest "${ARG_INSTALL_DESTINATION}")
                if(NOT _tree STREQUAL ".")
                    set(_install_dest "${_install_dest}/${_tree}")
                endif()
                install(DIRECTORY "${_src}/"
                        DESTINATION "${_install_dest}"
                        ${_component_arg}
                        USE_SOURCE_PERMISSIONS
                        PATTERN ".git" EXCLUDE)
            endif()
        endforeach()
    endforeach()
endfunction()
