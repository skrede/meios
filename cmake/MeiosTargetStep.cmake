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
