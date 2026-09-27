include_guard(GLOBAL)

# The key is the target's runtime directory because every write a deploy or flatten makes lands
# inside it while the paths inside it nest. The name is a short digest so the path stays short on
# Windows, and the key is case-folded because the default Windows and macOS file systems fold case,
# while two directories sharing a lock cost only waiting.
function(_meios_step_lock_path binary_dir root out)
    cmake_path(SET _key NORMALIZE "${root}")
    string(TOLOWER "${_key}" _key)
    string(SHA256 _digest "${_key}")
    string(SUBSTRING "${_digest}" 0 16 _digest)
    set(${out} "${binary_dir}/CMakeFiles/meios-locks/${_digest}.lock" PARENT_SCOPE)
endfunction()

# The operating system releases the lock when its holder exits, so a crashed step leaves nothing
# held. Without a bound the wait blocks and wakes on release; with one, CMake polls once a second,
# which is why no bound is the default. "Timeout reached" is cmFileLockResult.cxx's text.
function(_meios_step_lock binary_dir root timeout out)
    _meios_step_lock_path("${binary_dir}" "${root}" _path)
    set(_bound "")
    if(timeout)
        set(_bound TIMEOUT ${timeout})
    endif()
    file(LOCK "${_path}" GUARD PROCESS ${_bound} RESULT_VARIABLE _result)
    if(_result STREQUAL "Timeout reached")
        message(FATAL_ERROR
            "meios: waited ${timeout} seconds for another step writing into '${root}' to release "
            "${_path}. That step is still running, since a lock is released when its process "
            "exits. If it is copying a large tree, raise MEIOS_STEP_LOCK_TIMEOUT, or leave it "
            "empty to wait for as long as the step takes.")
    elseif(NOT _result STREQUAL "0")
        message(FATAL_ERROR
            "meios: could not lock ${_path} for a step writing into '${root}': ${_result}")
    endif()
    set(${out} "${_path}" PARENT_SCOPE)
endfunction()
