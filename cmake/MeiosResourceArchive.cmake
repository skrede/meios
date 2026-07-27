include_guard(GLOBAL)

function(_meios_url_download_args name hash out)
    set(_args "")
    if(hash)
        if(NOT hash MATCHES "^(SHA256|SHA512|SHA384|SHA224|SHA1)=[0-9a-fA-F]+$")
            message(FATAL_ERROR
                "meios_declare_resource(${name}): HASH must be <ALGO>=<hex> "
                "(e.g. SHA256=ab…); got '${hash}'.")
        endif()
        list(APPEND _args EXPECTED_HASH "${hash}")
    endif()
    if(MEIOS_RESOURCE_TLS_CAINFO)
        list(APPEND _args TLS_CAINFO "${MEIOS_RESOURCE_TLS_CAINFO}")
    endif()
    set(${out} "${_args}" PARENT_SCOPE)
endfunction()

function(_meios_url_download name url hash archive)
    _meios_url_download_args("${name}" "${hash}" _args)
    # TLS_VERIFY defaults OFF below CMake 3.31; the project floor is 3.28, so it is passed
    # explicitly on every call and no public knob disables it.
    file(DOWNLOAD "${url}" "${archive}"
         ${_args}
         TLS_VERIFY ON
         SHOW_PROGRESS
         INACTIVITY_TIMEOUT 60
         STATUS _dl_status
         LOG     _dl_log)
    list(GET _dl_status 0 _dl_code)
    list(GET _dl_status 1 _dl_msg)
    if(NOT _dl_code EQUAL 0)
        # Only a transport failure reaches here: file(DOWNLOAD ... EXPECTED_HASH) treats a
        # mismatch as fatal and aborts even with STATUS supplied, so control never returns for
        # one. The partial file is removed so it cannot survive to look like a cache entry.
        file(REMOVE "${archive}")
        message(FATAL_ERROR
            "meios_declare_resource(${name}): download failed "
            "(code ${_dl_code}: ${_dl_msg})\n  url:  ${url}\n  log:\n${_dl_log}")
    endif()
endfunction()

# Warned after the download rather than before it so the message can carry the hash that
# silences it: the only thing the caller is missing is a value they would otherwise have to
# compute by hand.
function(_meios_url_warn_unpinned name archive)
    file(SHA256 "${archive}" _computed)
    message(WARNING
        "meios_declare_resource(${name}): fetched WITHOUT an integrity hash. The download "
        "is trusted purely on TLS and server honesty, a swapped artifact is accepted silently, "
        "and nothing is cached, so every configure re-downloads. Pin it by adding:\n"
        "    HASH SHA256=${_computed}")
endfunction()

# Publish atomically: extraction happens in a scratch dir we own, so a crash mid-extract cannot
# leave a half-tree that looks cached. Extraction under an owned scratch dir also confines the blast
# radius of a hostile '../' entry (CMake rejects traversal only in 4.3+).
function(_meios_url_publish archive strip dest)
    set(_extract_tmp "${dest}.extract")
    file(REMOVE_RECURSE "${_extract_tmp}")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${_extract_tmp}")

    set(_extracted "${_extract_tmp}")
    if(strip)
        file(GLOB _top LIST_DIRECTORIES TRUE "${_extract_tmp}/*")
        list(LENGTH _top _n)
        if(_n EQUAL 1)
            list(GET _top 0 _top0)
            if(IS_DIRECTORY "${_top0}")
                set(_extracted "${_top0}")
            endif()
        endif()
    endif()

    file(REMOVE_RECURSE "${dest}")
    file(RENAME "${_extracted}" "${dest}")
    file(REMOVE_RECURSE "${_extract_tmp}")
    # The extracted tree is the cache; keeping the archive beside it stores every resource
    # twice for the sake of an extract that only happens if the tree is deleted by hand.
    file(REMOVE "${archive}")
endfunction()

function(_meios_url_acquire name url hash strip dest)
    set(_archive "${dest}.download")
    _meios_url_download("${name}" "${url}" "${hash}" "${_archive}")
    if(NOT hash)
        _meios_url_warn_unpinned("${name}" "${_archive}")
    endif()
    _meios_url_publish("${_archive}" "${strip}" "${dest}")
endfunction()
