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
    set(one_value     NAME URL HASH GIT_REPOSITORY GIT_TAG SOURCE_DIR SUBDIR OUT_DIR)
    set(multi_value)
    cmake_parse_arguments(ARG "${options}" "${one_value}" "${multi_value}" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "meios_declare_resource: NAME is required.")
    endif()
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "meios_declare_resource(${ARG_NAME}): unknown args: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    # Declared per resource so an offline or air-gapped configure can redirect any mode at a
    # pre-placed tree without editing the listfile that declares it.
    set(MEIOS_RESOURCE_${ARG_NAME}_SOURCE_DIR "" CACHE PATH
        "Pre-placed tree overriding acquisition of the '${ARG_NAME}' resource.")
    if(MEIOS_RESOURCE_${ARG_NAME}_SOURCE_DIR)
        set(ARG_SOURCE_DIR "${MEIOS_RESOURCE_${ARG_NAME}_SOURCE_DIR}")
        set(ARG_URL "")
        set(ARG_GIT_REPOSITORY "")
    endif()

    set(_modes 0)
    foreach(_m SOURCE_DIR URL GIT_REPOSITORY)
        if(ARG_${_m})
            math(EXPR _modes "${_modes}+1")
        endif()
    endforeach()
    if(NOT _modes EQUAL 1)
        message(FATAL_ERROR
            "meios_declare_resource(${ARG_NAME}): specify exactly one of "
            "SOURCE_DIR / URL / GIT_REPOSITORY.")
    endif()

    set(_root "${MEIOS_RESOURCE_CACHE_DIR}")
    if(NOT _root)
        set(_root "${CMAKE_BINARY_DIR}/_meios_resources")
    endif()
    file(MAKE_DIRECTORY "${_root}")
    set(_source_dir "${_root}/${ARG_NAME}")

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

    # Stamp cache keyed on the integrity anchor (hash for URL mode, tag for Git mode). An empty key
    # (no hash supplied) never matches, forcing a re-fetch — there is no trustworthy cache key without one.
    if(ARG_URL)
        set(_want "${ARG_HASH}")
    else()
        set(_want "git:${ARG_GIT_TAG}")
    endif()
    set(_stamp "${_root}/${ARG_NAME}.stamp")
    set(_cached FALSE)
    if(EXISTS "${_stamp}" AND IS_DIRECTORY "${_source_dir}" AND NOT _want STREQUAL "")
        file(READ "${_stamp}" _have)
        if(_have STREQUAL "${_want}")
            set(_cached TRUE)
        endif()
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
        else()
            message(WARNING
                "meios_declare_resource(${ARG_NAME}): fetching WITHOUT an integrity hash. The download "
                "is trusted purely on TLS and server honesty; a swapped artifact is accepted silently. "
                "Pin HASH SHA256=… for a reproducible, tamper-evident fetch.")
        endif()

        set(_tls_ca_arg "")
        if(MEIOS_RESOURCE_TLS_CAINFO)
            set(_tls_ca_arg TLS_CAINFO "${MEIOS_RESOURCE_TLS_CAINFO}")
        endif()

        set(_archive "${_root}/${ARG_NAME}.download")
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

        set(_extract_tmp "${_root}/${ARG_NAME}.extract")
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
    elseif(NOT _cached)
        find_package(Git REQUIRED)
        set(_branch_arg "")
        if(ARG_GIT_TAG)
            set(_branch_arg --branch "${ARG_GIT_TAG}")
        endif()
        file(REMOVE_RECURSE "${_source_dir}")
        # Escape hatch for LFS, submodules, and private auth. Large-file objects are left as their
        # pointers and submodules are left uninitialized on purpose: the pointer scan below fails
        # loudly rather than silently pulling stub geometry into the tree.
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" clone --depth 1 ${_branch_arg}
                    -- "${ARG_GIT_REPOSITORY}" "${_source_dir}"
            RESULT_VARIABLE _rc
            ERROR_VARIABLE  _err)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR
                "meios_declare_resource(${ARG_NAME}): git clone failed: ${_err}")
        endif()
    endif()

    set(_tree "${_source_dir}")
    if(ARG_SUBDIR)
        _meios_resource_subdir("${ARG_NAME}" "${_tree}" "${ARG_SUBDIR}" _tree)
    endif()

    if(NOT _cached)
        _meios_scan_lfs_pointers("${ARG_NAME}" "${_tree}")
        file(WRITE "${_stamp}" "${_want}")
    endif()

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
    set(multi_value   RESOURCES)
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

    set(_dest "$<TARGET_FILE_DIR:${target}>")
    if(ARG_SUBDIR)
        if(IS_ABSOLUTE "${ARG_SUBDIR}")
            message(FATAL_ERROR
                "meios_target_deploy_resources(${target}): SUBDIR must be relative; got '${ARG_SUBDIR}'.")
        endif()
        set(_dest "${_dest}/${ARG_SUBDIR}")
    endif()

    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _slot "${ARG_SUBDIR}")

    foreach(_name IN LISTS ARG_RESOURCES)
        meios_resource_dir("${_name}" _dir)

        # Driven by an OUTPUT rule keyed on the tree's contents rather than a POST_BUILD command:
        # POST_BUILD runs only when the target itself relinks, so editing a resource without
        # touching a source file would leave a stale tree deployed with no sign anything was wrong.
        # copy_directory_if_different keeps an unchanged tree from restamping every file.
        file(GLOB_RECURSE _files CONFIGURE_DEPENDS "${_dir}/*")
        set(_stamp "${CMAKE_CURRENT_BINARY_DIR}/${target}.${_slot}.${_name}.deployed")
        add_custom_command(
            OUTPUT  "${_stamp}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${_dir}" "${_dest}"
            COMMAND ${CMAKE_COMMAND} -E touch "${_stamp}"
            DEPENDS ${_files}
            COMMENT "Deploying resource '${_name}' to ${_dest}"
            VERBATIM)
        add_custom_target(${target}_deploy_${_slot}_${_name} DEPENDS "${_stamp}")
        add_dependencies(${target} ${target}_deploy_${_slot}_${_name})

        if(ARG_INSTALL_DESTINATION)
            set(_component_arg "")
            if(ARG_INSTALL_COMPONENT)
                set(_component_arg COMPONENT "${ARG_INSTALL_COMPONENT}")
            endif()
            install(DIRECTORY "${_dir}/"
                    DESTINATION "${ARG_INSTALL_DESTINATION}"
                    ${_component_arg}
                    USE_SOURCE_PERMISSIONS
                    PATTERN ".git" EXCLUDE)
        endif()
    endforeach()
endfunction()
