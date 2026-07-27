include_guard(GLOBAL)

# A flattened document is a build-time deployment artifact: one description expanded and resolved
# once, written where a program will find it on disk. It is not this library's consumption path —
# a consumer that wants a resolved model asks the library for one and never reads a file written
# here.

include("${CMAKE_CURRENT_LIST_DIR}/MeiosResourcePaths.cmake")

# Captured while the module is being read: inside a function body the current list directory is the
# caller's, so a function locating its sibling script from there would look beside whichever
# listfile called it.
set(MEIOS_CMAKE_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}")

set(MEIOS_FLATTEN_EVAL_BACKENDS core python)

# Caller-supplied on both of its paths and never a discovery mechanism: it is the seam a
# cross-compiling build uses to name a binary that runs on the build host, and it is equally how a
# build with no meios package to look up points the rule at a binary it already has.
set(MEIOS_CLI_EXECUTABLE "" CACHE FILEPATH
    "Host-runnable meios binary used by the build-time flatten rule.")

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

# The order is fixed and stops where it stops. Nothing searches the path for a binary — one found
# there is version skew between this module and whatever expansion semantics that binary carries —
# and nothing turns the tools option on, because a function call that quietly rewrites a deliberate
# build setting is not a diagnostic, and it would pull the binary's own argument-parsing dependency
# into a build that declined it.
function(_meios_flatten_cli target out)
    # Without this the failure surfaces as an exec-format error from the build tool.
    if(CMAKE_CROSSCOMPILING AND NOT MEIOS_CLI_EXECUTABLE)
        message(FATAL_ERROR
            "meios_target_flatten_resource(${target}): the flatten runs on the build host, while "
            "the meios binary this build produces is built for the target. Set "
            "MEIOS_CLI_EXECUTABLE to a host-runnable meios binary.")
    endif()
    if(MEIOS_CLI_EXECUTABLE)
        set(${out} "${MEIOS_CLI_EXECUTABLE}" PARENT_SCOPE)
    elseif(TARGET meios::cli)
        set(${out} meios::cli PARENT_SCOPE)
    elseif(TARGET meios)
        set(${out} meios PARENT_SCOPE)
    else()
        message(FATAL_ERROR
            "meios_target_flatten_resource(${target}): this build has no meios binary to run the "
            "flatten with. Configure with MEIOS_BUILD_TOOLS=ON, or set MEIOS_CLI_EXECUTABLE to a "
            "meios binary you already have.")
    endif()
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
    list(GET MEIOS_FLATTEN_EVAL_BACKENDS 0 _default)
    if(NOT eval)
        set(eval "${_default}")
    endif()
    if(NOT "${eval}" IN_LIST MEIOS_FLATTEN_EVAL_BACKENDS)
        string(REPLACE ";" ", " _list "${MEIOS_FLATTEN_EVAL_BACKENDS}")
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

# Expand one description out of a declared resource tree. INPUT is relative to that tree; OUTPUT is
# relative to the target's runtime directory, the root meios_target_deploy_resources writes into.
function(meios_target_flatten_resource target)
    set(options       INSTALL_RUNTIME_RELATIVE)
    set(one_value     RESOURCE INPUT OUTPUT EVAL INSTALL_DESTINATION INSTALL_COMPONENT)
    set(multi_value   ARGS PACKAGE_PATH)
    cmake_parse_arguments(ARG "${options}" "${one_value}" "${multi_value}" ${ARGN})
    _meios_flatten_validate("${target}" "${ARG_UNPARSED_ARGUMENTS}" "${ARG_RESOURCE}"
                            "${ARG_INPUT}" "${ARG_OUTPUT}")
    _meios_flatten_check_install("${target}" "${ARG_INSTALL_RUNTIME_RELATIVE}"
                                 "${ARG_INSTALL_DESTINATION}" "${ARG_INSTALL_COMPONENT}")
    _meios_flatten_paths("${target}" "${ARG_INPUT}" "${ARG_OUTPUT}" "${ARG_PACKAGE_PATH}")
    _meios_flatten_check_args("${target}" "${ARG_ARGS}")
    _meios_flatten_cli("${target}" _cli)
    _meios_flatten_eval("${target}" "${ARG_EVAL}" _eval)
    _meios_flatten_announce("${target}" "${ARG_INPUT}" "${_eval}")
endfunction()
