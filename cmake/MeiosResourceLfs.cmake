include_guard(GLOBAL)

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
