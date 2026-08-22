cmake_minimum_required(VERSION 3.28)

# git reads a plain absolute path as implying --local and then prints "--depth is ignored in local
# clones" on every acquisition, because the module hardcodes --depth 1; the file:// spelling takes
# the ordinary transport instead. A Windows path needs the extra leading slash: C:/x becomes /C:/x.
function(meios_harness_file_url path out)
    file(TO_CMAKE_PATH "${path}" _p)
    if(NOT _p MATCHES "^/")
        set(_p "/${_p}")
    endif()
    set(${out} "file://${_p}" PARENT_SCOPE)
endfunction()

# Identity, signing, hook path and exclude file travel on the command line and the timestamps in the
# environment, so a developer's global git configuration cannot change what a case sees.
function(meios_harness_git dir)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                GIT_AUTHOR_DATE=2020-01-01T00:00:00Z
                GIT_COMMITTER_DATE=2020-01-01T00:00:00Z
                -- "${GIT_EXECUTABLE}"
                -c user.name=meios-harness
                -c user.email=harness@example.invalid
                -c commit.gpgsign=false
                -c core.hooksPath=
                -c core.excludesFile=
                -C "${dir}" ${ARGN}
        RESULT_VARIABLE _rc
        ERROR_VARIABLE  _err)
    if(NOT _rc EQUAL 0)
        string(REPLACE ";" " " _what "${ARGN}")
        message(FATAL_ERROR "meios-harness: git ${_what} failed in ${dir}:\n${_err}")
    endif()
endfunction()

# Not --bare: the tree has to be staged from a worktree, and file:// serves a non-bare repository
# perfectly well. '-b main' keeps init.defaultBranch advice out of the transcript and makes a ref
# assertion deterministic. uploadpack.allowFilter is what lets the module's --filter=blob:none
# perform a real partial clone rather than warn and fall back to a whole one.
function(meios_harness_git_origin source dest out_url)
    find_package(Git REQUIRED)
    file(REMOVE_RECURSE "${dest}")
    file(COPY "${source}/" DESTINATION "${dest}")
    meios_harness_git("${dest}" init -q -b main)
    meios_harness_git("${dest}" config uploadpack.allowFilter true)
    meios_harness_git("${dest}" add -A)
    meios_harness_git("${dest}" commit -q -m "harness origin")
    meios_harness_git("${dest}" tag v1)
    meios_harness_file_url("${dest}" _url)
    set(${out_url} "${_url}" PARENT_SCOPE)
endfunction()

# The archive is written through `cmake -E tar` rather than file(ARCHIVE_CREATE) because only the
# former can be told which directory the entry names are relative to on the project's CMake floor:
# file(ARCHIVE_CREATE) gained WORKING_DIRECTORY in 3.31 and otherwise names entries relative to the
# process working directory, which would bury the packages under the build path.
#
# The digest is taken in the same run that writes the archive, and has to be: a tar header carries
# each entry's mtime, so two runs a second apart produce different bytes and a digest committed
# beside the case would pass once and then fail forever.
function(meios_harness_tarball_origin source dest out_url out_hash)
    file(GLOB _entries LIST_DIRECTORIES TRUE RELATIVE "${source}" "${source}/*")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar czf "${dest}" ${_entries}
        WORKING_DIRECTORY "${source}"
        RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "meios-harness: could not build an archive of ${source} at ${dest}")
    endif()
    file(SHA256 "${dest}" _hash)
    meios_harness_file_url("${dest}" _url)
    set(${out_url}  "${_url}" PARENT_SCOPE)
    set(${out_hash} "${_hash}" PARENT_SCOPE)
endfunction()
