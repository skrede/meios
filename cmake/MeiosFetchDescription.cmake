include_guard(GLOBAL)

# Fetch a robot-description package as DATA (parsed, never compiled, never linked).
# Every acquisition mode resolves to a directory on disk consumed as source_kind::directory.

set(MEIOS_FETCH_CACHE_DIR "" CACHE PATH
    "Root directory for fetched robot-description packages (defaults to the build tree).")
set(MEIOS_FETCH_TLS_CAINFO "" CACHE FILEPATH
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
            "meios_fetch_description(${name}): unsmudged Git-LFS pointers found — these meshes are "
            "text stubs, not geometry:\n    ${_plist}\n"
            "Fetch a properly-smudged archive, or use GIT_REPOSITORY mode with Git-LFS smudging enabled.")
    endif()
endfunction()

function(meios_fetch_description)
    set(options       STRIP_TOP_LEVEL INSTALL)
    set(one_value     NAME URL HASH GIT_REPOSITORY GIT_TAG SOURCE_DIR
                      INSTALL_DESTINATION INSTALL_COMPONENT OUT_DIR)
    set(multi_value)
    cmake_parse_arguments(ARG "${options}" "${one_value}" "${multi_value}" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "meios_fetch_description: NAME is required.")
    endif()
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "meios_fetch_description(${ARG_NAME}): unknown args: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT ARG_OUT_DIR)
        message(FATAL_ERROR "meios_fetch_description(${ARG_NAME}): OUT_DIR is required.")
    endif()

    set(_modes 0)
    foreach(_m SOURCE_DIR URL GIT_REPOSITORY)
        if(ARG_${_m})
            math(EXPR _modes "${_modes}+1")
        endif()
    endforeach()
    if(NOT _modes EQUAL 1)
        message(FATAL_ERROR
            "meios_fetch_description(${ARG_NAME}): specify exactly one of "
            "SOURCE_DIR / URL / GIT_REPOSITORY.")
    endif()

    set(_root "${MEIOS_FETCH_CACHE_DIR}")
    if(NOT _root)
        set(_root "${CMAKE_BINARY_DIR}/_meios_descriptions")
    endif()
    file(MAKE_DIRECTORY "${_root}")
    set(_source_dir "${_root}/${ARG_NAME}")

    # Pre-populated override: skip all fetching, but still scan — a hand-placed tree can be unsmudged.
    if(ARG_SOURCE_DIR)
        if(NOT IS_DIRECTORY "${ARG_SOURCE_DIR}")
            message(FATAL_ERROR
                "meios_fetch_description(${ARG_NAME}): SOURCE_DIR '${ARG_SOURCE_DIR}' is not a directory.")
        endif()
        _meios_scan_lfs_pointers("${ARG_NAME}" "${ARG_SOURCE_DIR}")
        set(${ARG_OUT_DIR} "${ARG_SOURCE_DIR}" PARENT_SCOPE)
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
    if(EXISTS "${_stamp}" AND IS_DIRECTORY "${_source_dir}" AND NOT _want STREQUAL "")
        file(READ "${_stamp}" _have)
        if(_have STREQUAL "${_want}")
            set(${ARG_OUT_DIR} "${_source_dir}" PARENT_SCOPE)
            return()
        endif()
    endif()

    if(ARG_URL)
        set(_hash_arg "")
        if(ARG_HASH)
            if(NOT ARG_HASH MATCHES "^(SHA256|SHA512|SHA384|SHA224|SHA1)=[0-9a-fA-F]+$")
                message(FATAL_ERROR
                    "meios_fetch_description(${ARG_NAME}): HASH must be <ALGO>=<hex> "
                    "(e.g. SHA256=ab…); got '${ARG_HASH}'.")
            endif()
            set(_hash_arg EXPECTED_HASH "${ARG_HASH}")
        else()
            message(WARNING
                "meios_fetch_description(${ARG_NAME}): fetching WITHOUT an integrity hash. The download "
                "is trusted purely on TLS and server honesty; a swapped artifact is accepted silently. "
                "Pin HASH SHA256=… for a reproducible, tamper-evident fetch.")
        endif()

        set(_tls_ca_arg "")
        if(MEIOS_FETCH_TLS_CAINFO)
            set(_tls_ca_arg TLS_CAINFO "${MEIOS_FETCH_TLS_CAINFO}")
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
                "meios_fetch_description(${ARG_NAME}): download failed "
                "(code ${_dl_code}: ${_dl_msg})\n  url:  ${ARG_URL}\n  log:\n${_dl_log}")
        endif()

        set(_extract_tmp "${_root}/${ARG_NAME}.extract")
        file(REMOVE_RECURSE "${_extract_tmp}")
        file(ARCHIVE_EXTRACT INPUT "${_archive}" DESTINATION "${_extract_tmp}")

        set(_tree "${_extract_tmp}")
        if(ARG_STRIP_TOP_LEVEL)
            file(GLOB _top LIST_DIRECTORIES TRUE "${_extract_tmp}/*")
            list(LENGTH _top _n)
            if(_n EQUAL 1)
                list(GET _top 0 _top0)
                if(IS_DIRECTORY "${_top0}")
                    set(_tree "${_top0}")
                endif()
            endif()
        endif()

        # Publish atomically: extraction happened in a scratch dir we own, so a crash mid-extract
        # cannot leave a half-tree that looks cached. Extraction under an owned scratch dir also
        # confines the blast radius of a hostile '../' entry (CMake rejects traversal only in 4.3+).
        file(REMOVE_RECURSE "${_source_dir}")
        file(RENAME "${_tree}" "${_source_dir}")
        file(REMOVE_RECURSE "${_extract_tmp}")
    else()
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
                "meios_fetch_description(${ARG_NAME}): git clone failed: ${_err}")
        endif()
    endif()

    _meios_scan_lfs_pointers("${ARG_NAME}" "${_source_dir}")

    if(ARG_INSTALL)
        install(DIRECTORY "${_source_dir}/"
                DESTINATION "${ARG_INSTALL_DESTINATION}"
                COMPONENT   "${ARG_INSTALL_COMPONENT}"
                USE_SOURCE_PERMISSIONS
                PATTERN ".git" EXCLUDE)
    endif()

    file(WRITE "${_stamp}" "${_want}")
    set(${ARG_OUT_DIR} "${_source_dir}" PARENT_SCOPE)
endfunction()
