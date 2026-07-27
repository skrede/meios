include_guard(GLOBAL)

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
