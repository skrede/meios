include_guard(GLOBAL)

# A flattened document is a build-time deployment artifact: one description expanded and resolved
# once, written where a program will find it on disk. It is not this library's consumption path —
# a consumer that wants a resolved model asks the library for one and never reads a file written
# here.

include("${CMAKE_CURRENT_LIST_DIR}/MeiosFlattenArgs.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/MeiosDeployResources.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/MeiosResourceRegistry.cmake")

# Captured while the module is being read: inside a function body the current list directory is the
# caller's, so a function locating its sibling script from there would look beside whichever
# listfile called it. A GLOBAL property rather than a variable because include_guard runs this file
# once, in whichever directory reaches it first, and that directory's siblings never see a variable
# set there.
set_property(GLOBAL PROPERTY MEIOS_CMAKE_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}")

# Caller-supplied on both of its paths and never a discovery mechanism: it is the seam a
# cross-compiling build uses to name a binary that runs on the build host, and it is equally how a
# build with no meios package to look up points the rule at a binary it already has.
set(MEIOS_CLI_EXECUTABLE "" CACHE FILEPATH
    "Host-runnable meios binary used by the build-time flatten rule.")

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

function(_meios_flatten_input target resource input out)
    meios_resource_dir("${resource}" _dir)
    if(NOT EXISTS "${_dir}/${input}")
        message(FATAL_ERROR
            "meios_target_flatten_resource(${target}): INPUT '${input}' names no file in "
            "resource '${resource}':\n  ${_dir}/${input}")
    endif()
    set(${out} "${_dir}" PARENT_SCOPE)
endfunction()

# PACKAGE_PATH entries are relative to the acquired tree, which is itself always a search root:
# a package:// reference in the description resolves against the tree it was acquired with.
function(_meios_flatten_argv dir input args package_path eval out)
    set(_argv --eval "${eval}" --package-path "${dir}")
    foreach(_root IN LISTS package_path)
        list(APPEND _argv --package-path "${dir}/${_root}")
    endforeach()
    list(APPEND _argv "${dir}/${input}" ${args})
    set(${out} "${_argv}" PARENT_SCOPE)
endfunction()

# The rule runs on every build because an edit to a description changes no source file: a POST_BUILD
# command runs only when the target relinks and would leave a stale document behind, and a
# modification-time comparison answers a different question than whether the document is current.
# The repeated run stays cheap because the run script publishes only a document whose bytes differ.
function(_meios_flatten_command target cli argv out_file rule)
    set(_run "${cli}")
    if(TARGET ${cli})
        set(_run "$<TARGET_FILE:${cli}>")
    endif()
    # List expansion of the command splits every unescaped ';' in an argument, so the vector has to
    # arrive escaped or the wrapper would receive its first element and nothing else. $<SEMICOLON>
    # does not survive it either: it is evaluated before the split.
    string(REPLACE ";" "\;" _args "${argv}")
    get_property(_module_dir GLOBAL PROPERTY MEIOS_CMAKE_MODULE_DIR)
    add_custom_target(${rule}
        COMMAND ${CMAKE_COMMAND} "-DMEIOS_FLATTEN_CLI=${_run}" "-DMEIOS_FLATTEN_ARGS=${_args}"
                "-DMEIOS_FLATTEN_OUT=${out_file}"
                -P "${_module_dir}/MeiosFlattenRun.cmake"
        COMMENT "Flattening ${out_file}"
        VERBATIM COMMAND_EXPAND_LISTS)
    # An always-run target carries no file dependency, so what used to order the flatten after the
    # binary it drives is now a target dependency. There is none to add when the caller supplied a
    # path instead of a target.
    if(TARGET ${cli})
        add_dependencies(${rule} ${cli})
    endif()
endfunction()

function(_meios_flatten_rule target resource input output args package_path cli eval out)
    _meios_flatten_input("${target}" "${resource}" "${input}" _dir)
    _meios_flatten_argv("${_dir}" "${input}" "${args}" "${package_path}" "${eval}" _argv)
    # A rule's output may not carry a target-dependent expression, which is why the document is
    # named through a command argument rather than declared as the rule's output.
    set(_file "$<TARGET_FILE_DIR:${target}>/${output}")
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _slug "${output}")
    _meios_flatten_command("${target}" "${cli}" "${_argv}" "${_file}" ${target}_flatten_${_slug})
    # Ordering onto the deploy rule is a target dependency rather than a file dependency, because a
    # file dependency only connects two commands issued in one directory and flatten may be called
    # from another.
    get_property(_deploy GLOBAL PROPERTY MEIOS_DEPLOY_TARGETS_${target}_${resource})
    if(_deploy)
        add_dependencies(${target}_flatten_${_slug} ${_deploy})
    endif()
    add_dependencies(${target} ${target}_flatten_${_slug})
    set(${out} "${_file}" PARENT_SCOPE)
endfunction()

function(_meios_flatten_install target output runtime_relative install_dest component out_file)
    get_filename_component(_subdir "${output}" DIRECTORY)
    _meios_deploy_destination("${target}" "${_subdir}" "${runtime_relative}" "${component}"
                              "${install_dest}" _dest _unused_dir _unused_slot)
    if(NOT _dest)
        return()
    endif()
    set(_component_arg "")
    if(component)
        set(_component_arg COMPONENT "${component}")
    endif()
    install(FILES "${out_file}" DESTINATION "${_dest}" ${_component_arg})
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
    _meios_flatten_rule("${target}" "${ARG_RESOURCE}" "${ARG_INPUT}" "${ARG_OUTPUT}" "${ARG_ARGS}"
                        "${ARG_PACKAGE_PATH}" "${_cli}" "${_eval}" _out)
    _meios_flatten_install("${target}" "${ARG_OUTPUT}" "${ARG_INSTALL_RUNTIME_RELATIVE}"
                           "${ARG_INSTALL_DESTINATION}" "${ARG_INSTALL_COMPONENT}" "${_out}")
endfunction()
