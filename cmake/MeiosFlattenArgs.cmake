include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/MeiosResourcePaths.cmake")

# A GLOBAL property rather than a plain variable, for the reason the registry gives: include_guard
# runs this file once, in whichever directory reaches it first, and a variable set there is invisible
# to that directory's siblings — so a consumer that finds the package in one subdirectory and calls
# the function from another would read an empty vocabulary.
set_property(GLOBAL PROPERTY MEIOS_FLATTEN_EVAL_BACKENDS core python)

function(_meios_flatten_validate target unparsed resource input output)
    if(NOT TARGET ${target})
        message(FATAL_ERROR
            "meios_target_flatten_resource: '${target}' is not a target.")
    endif()
    if(unparsed)
        message(FATAL_ERROR
            "meios_target_flatten_resource(${target}): unknown args: ${unparsed}")
    endif()
    if(NOT resource)
        message(FATAL_ERROR
            "meios_target_flatten_resource(${target}): RESOURCE is required.")
    endif()
    if(NOT input)
        message(FATAL_ERROR
            "meios_target_flatten_resource(${target}): INPUT names the description to expand, "
            "relative to the resource tree; it is required.")
    endif()
    if(NOT output)
        message(FATAL_ERROR
            "meios_target_flatten_resource(${target}): OUTPUT is required.")
    endif()
endfunction()

function(_meios_flatten_check_install target runtime_relative install_dest component)
    if(runtime_relative AND install_dest)
        message(FATAL_ERROR
            "meios_target_flatten_resource(${target}): INSTALL_RUNTIME_RELATIVE and "
            "INSTALL_DESTINATION both name an install location; pass one.")
    endif()
    if(component AND NOT runtime_relative AND NOT install_dest)
        message(FATAL_ERROR
            "meios_target_flatten_resource(${target}): INSTALL_COMPONENT requires "
            "INSTALL_DESTINATION or INSTALL_RUNTIME_RELATIVE.")
    endif()
endfunction()

function(_meios_flatten_paths target input output package_path)
    set(_context "meios_target_flatten_resource(${target})")
    _meios_check_contained_paths("${_context}" INPUT "${input}")
    _meios_check_contained_paths("${_context}" OUTPUT "${output}")
    _meios_check_contained_paths("${_context}" PACKAGE_PATH "${package_path}")
endfunction()

# Two distinct failures arrive as an ill-formed entry. A leading '-' reaches the binary as an
# option rather than an override, and a ';' inside a value splits the entry into two list elements
# before this function is ever entered, so the second half turns up carrying no assignment at all.
function(_meios_flatten_check_args target args)
    foreach(_arg IN LISTS args)
        if(_arg MATCHES "^-")
            message(FATAL_ERROR
                "meios_target_flatten_resource(${target}): ARGS entry '${_arg}' begins with '-', "
                "which the meios binary reads as an option; write it as key:=value.")
        endif()
        if(NOT _arg MATCHES "^.+:=")
            message(FATAL_ERROR
                "meios_target_flatten_resource(${target}): ARGS entry '${_arg}' is not "
                "key:=value. A ';' inside a value splits the entry in two before this function "
                "sees it; write the value without one.")
        endif()
    endforeach()
endfunction()

# Precedence, and the reason it is written this way: a cache entry of this name is a caller stating
# the fact deliberately, which is how both the refusal and the accepting path stay reachable on a
# machine that cannot build the enrichment. The property carries the answer otherwise, written by
# the binary's own listfile in-tree and by the substituted package config once installed. Only the
# CACHE test tells the injection apart from the plain variable the top-level build sets to feed
# that substitution.
function(_meios_flatten_eval_python_linked out)
    if(DEFINED CACHE{MEIOS_CLI_HAS_EVAL_PYTHON})
        set(${out} "$CACHE{MEIOS_CLI_HAS_EVAL_PYTHON}" PARENT_SCOPE)
        return()
    endif()
    get_property(_linked GLOBAL PROPERTY MEIOS_CLI_HAS_EVAL_PYTHON)
    set(${out} "${_linked}" PARENT_SCOPE)
endfunction()

function(_meios_flatten_eval target eval out)
    get_property(_backends GLOBAL PROPERTY MEIOS_FLATTEN_EVAL_BACKENDS)
    list(GET _backends 0 _default)
    if(NOT eval)
        set(eval "${_default}")
    endif()
    if(NOT "${eval}" IN_LIST _backends)
        string(REPLACE ";" ", " _list "${_backends}")
        message(FATAL_ERROR
            "meios_target_flatten_resource(${target}): EVAL '${eval}' is not an evaluator "
            "backend. Pass one of: ${_list}.")
    endif()
    if(eval STREQUAL "python")
        _meios_flatten_eval_python_linked(_linked)
        if(NOT _linked)
            message(FATAL_ERROR
                "meios_target_flatten_resource(${target}): EVAL python needs a meios binary "
                "built with the python evaluator, and this one is not. Configure with "
                "MEIOS_BUILD_EVAL_PYTHON=ON, or pass EVAL ${_default}.")
        endif()
    endif()
    set(${out} "${eval}" PARENT_SCOPE)
endfunction()

# Informative, never a gate: a second opt-in variable would be a second lock on a door the consumer
# just chose to open by writing the backend into their own listfile.
function(_meios_flatten_announce target input eval)
    set(_note "")
    if(eval STREQUAL "python")
        set(_note ", which executes Python during the build")
    endif()
    message(STATUS
        "meios flatten (${target}): ${input} with the ${eval} evaluator${_note}")
endfunction()
