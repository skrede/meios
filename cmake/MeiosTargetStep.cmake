include_guard(GLOBAL)

# Each target carries one step: a custom command whose output is never written, listed as a source
# of the consumer's own target, to which every deploy and flatten call appends its commands in call
# order. The absent output runs the step on every build without adding a target, and since it names
# no file any generator compiles or links, the step never relinks the target.

function(_meios_step_check_directory context target)
    # The binary directory, not the source directory, identifies the scope a command is issued in.
    get_target_property(_binary_dir ${target} BINARY_DIR)
    if(NOT _binary_dir STREQUAL CMAKE_CURRENT_BINARY_DIR)
        get_target_property(_source_dir ${target} SOURCE_DIR)
        message(FATAL_ERROR
            "${context}: must be called in the directory that defines '${target}' (${_source_dir}), "
            "not ${CMAKE_CURRENT_SOURCE_DIR}. CMake builds a custom command only for a target "
            "defined in the directory that issues it, so a call from here would be dropped without "
            "a diagnostic.")
    endif()
endfunction()

# Only the target's name enters the output's name: the Visual Studio generator names a rule file
# after it, and a length the caller's arguments control would carry that path past the limit.
# VERBATIM and list expansion are settings of the command object, fixed by the call that creates it
# and applied to every command appended later, so they are fixed here rather than by whichever call
# comes first.
function(_meios_step_mode target out_step out_mode)
    set(_step "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target}.meios")
    get_property(_created GLOBAL PROPERTY MEIOS_STEP_${target} SET)
    if(_created)
        set(_mode APPEND)
    else()
        set_property(GLOBAL PROPERTY MEIOS_STEP_${target} "${_step}")
        set_source_files_properties("${_step}" PROPERTIES SYMBOLIC TRUE)
        target_sources(${target} PRIVATE "${_step}")
        set(_mode COMMENT "Placing meios resources beside ${target}" VERBATIM COMMAND_EXPAND_LISTS)
    endif()
    set(${out_step} "${_step}" PARENT_SCOPE)
    set(${out_mode} "${_mode}" PARENT_SCOPE)
endfunction()

set(MEIOS_STEP_LOCK_TIMEOUT "" CACHE STRING
    "Seconds a meios deploy or flatten step waits for another step writing into the same \
runtime directory; empty or 0 waits for as long as that step takes.")

# The directory of the file defining the function, because inside a function body the current list
# directory is the caller's.
function(_meios_step_script name out)
    set(${out} "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${name}" PARENT_SCOPE)
endfunction()

# CMAKE_BINARY_DIR is the top of the whole build, the same in every directory and every subproject,
# so every step of one build agrees on where the locks are without knowing about the others.
function(_meios_step_lock_args target out)
    if(NOT MEIOS_STEP_LOCK_TIMEOUT MATCHES "^[0-9]*$")
        message(FATAL_ERROR
            "meios: MEIOS_STEP_LOCK_TIMEOUT is '${MEIOS_STEP_LOCK_TIMEOUT}', which is not a whole "
            "number of seconds. Set a number of seconds, or leave it empty to wait for as long as "
            "the other step takes.")
    endif()
    set(${out}
        "-DMEIOS_STEP_ROOT=$<TARGET_FILE_DIR:${target}>"
        "-DMEIOS_STEP_BINARY_DIR=${CMAKE_BINARY_DIR}"
        "-DMEIOS_STEP_LOCK_TIMEOUT=${MEIOS_STEP_LOCK_TIMEOUT}"
        PARENT_SCOPE)
endfunction()
