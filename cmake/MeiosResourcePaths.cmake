include_guard(GLOBAL)

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

# Every caller-supplied path that is joined onto an acquired tree or a deployment root passes here.
# A leading '-' is rejected alongside traversal because these values also reach git as arguments,
# where a path that looks like an option is read as one.
function(_meios_check_contained_paths context what paths)
    foreach(_p IN LISTS paths)
        if(IS_ABSOLUTE "${_p}" OR _p MATCHES "(^|/)\\.\\.(/|$)" OR _p MATCHES "^-")
            message(FATAL_ERROR
                "${context}: ${what} entry '${_p}' must be a relative path inside the tree, with "
                "no '..' component and no leading '-'.")
        endif()
    endforeach()
endfunction()
