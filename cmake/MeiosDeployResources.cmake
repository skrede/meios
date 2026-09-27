include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/MeiosResourcePaths.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/MeiosResourceRegistry.cmake")

# No rule name carries a length the caller's arguments control, because the Visual Studio generator
# repeats it in the utility project's intermediate path, where such a length would reach the path
# limit. So a deploy rule is numbered per call and a flatten rule takes a digest of its output.
function(_meios_rule_name target kind identity out)
    string(SHA256 _digest "${identity}")
    string(SUBSTRING "${_digest}" 0 8 _digest)
    set(${out} "${target}_${kind}_${_digest}" PARENT_SCOPE)
endfunction()

# The count is global because a target's deploy calls may come from different directories.
function(_meios_deploy_next_rule target out)
    get_property(_calls GLOBAL PROPERTY MEIOS_DEPLOY_CALLS_${target})
    if(NOT _calls)
        set(_calls 0)
    endif()
    math(EXPR _calls "${_calls} + 1")
    set_property(GLOBAL PROPERTY MEIOS_DEPLOY_CALLS_${target} "${_calls}")
    if(_calls EQUAL 1)
        set(${out} "${target}_deploy" PARENT_SCOPE)
    else()
        set(${out} "${target}_deploy_${_calls}" PARENT_SCOPE)
    endif()
endfunction()

function(_meios_deploy_validate target unparsed resources runtime_relative install_dest)
    if(NOT TARGET ${target})
        message(FATAL_ERROR
            "meios_target_deploy_resources: '${target}' is not a target.")
    endif()
    if(unparsed)
        message(FATAL_ERROR
            "meios_target_deploy_resources(${target}): unknown args: ${unparsed}")
    endif()
    if(NOT resources)
        message(FATAL_ERROR
            "meios_target_deploy_resources(${target}): RESOURCES is required.")
    endif()
    if(runtime_relative AND install_dest)
        message(FATAL_ERROR
            "meios_target_deploy_resources(${target}): INSTALL_RUNTIME_RELATIVE and "
            "INSTALL_DESTINATION both name an install location; pass one.")
    endif()
endfunction()

# Mirrors SUBDIR under the runtime destination so a program that finds its resources relative to
# its own executable keeps working once installed. Installing the tree to a conventional data
# directory instead would install successfully and still break such a program at runtime, and
# only after install — the build tree resolves either way.
function(_meios_deploy_destination target subdir runtime_relative component install_dest
                                   out_install out_dest)
    if(runtime_relative)
        include(GNUInstallDirs)
        set(install_dest "${CMAKE_INSTALL_BINDIR}")
        if(subdir)
            set(install_dest "${install_dest}/${subdir}")
        endif()
    endif()
    if(component AND NOT install_dest)
        message(FATAL_ERROR
            "meios_target_deploy_resources(${target}): INSTALL_COMPONENT requires "
            "INSTALL_DESTINATION or INSTALL_RUNTIME_RELATIVE.")
    endif()
    set(_dest "$<TARGET_FILE_DIR:${target}>")
    if(subdir)
        set(_dest "${_dest}/${subdir}")
    endif()
    set(${out_install} "${install_dest}" PARENT_SCOPE)
    set(${out_dest} "${_dest}" PARENT_SCOPE)
endfunction()

# PACKAGES names entries to copy out of ONE tree, so more than one source tree would leave the
# name ambiguous. Filtering is a property of what gets shipped, not of the acquired tree, which
# is why it lives here rather than on the declaration.
# "." stands for the tree itself. An empty string cannot serve as the sentinel: a list whose
# only element is "" is an empty list to foreach(IN LISTS), which would silently deploy
# nothing at all whenever PACKAGES was absent.
function(_meios_deploy_selection target name dir resources packages out)
    list(LENGTH resources _resource_count)
    if(packages AND NOT _resource_count EQUAL 1)
        message(FATAL_ERROR
            "meios_target_deploy_resources(${target}): PACKAGES selects entries from a single "
            "resource tree; pass exactly one RESOURCES name (got ${_resource_count}).")
    endif()
    if(NOT packages)
        set(${out} "." PARENT_SCOPE)
        return()
    endif()
    foreach(_pkg IN LISTS packages)
        if(NOT IS_DIRECTORY "${dir}/${_pkg}")
            message(FATAL_ERROR
                "meios_target_deploy_resources(${target}): resource '${name}' has no "
                "entry '${_pkg}' to select:\n  ${dir}/${_pkg}")
        endif()
    endforeach()
    set(${out} "${packages}" PARENT_SCOPE)
endfunction()

# The rule runs on every build because copy_directory_if_different decides currency by comparing
# content, and a modification-time comparison in front of it answers a different question than
# whether the deployed tree is current. A POST_BUILD command stays rejected for the reason it always
# was: it runs only when the target itself relinks, so editing a resource without touching a source
# file would leave a stale tree deployed with no sign anything was wrong.
function(_meios_deploy_command target rule resources labels dest commands)
    list(JOIN labels ", " _labels)
    add_custom_target(${rule} ${commands}
        COMMENT "Deploying ${_labels} to ${dest}"
        VERBATIM)
    add_dependencies(${target} ${rule})
    # Published rather than left to be reconstructed: a module ordering against this rule would
    # otherwise couple itself to a target-name format that the next edit here breaks.
    foreach(_name IN LISTS resources)
        set_property(GLOBAL APPEND PROPERTY MEIOS_DEPLOY_TARGETS_${target}_${_name} "${rule}")
    endforeach()
endfunction()

function(_meios_deploy_install install_dest component tree src)
    set(_component_arg "")
    if(component)
        set(_component_arg COMPONENT "${component}")
    endif()
    set(_install_dest "${install_dest}")
    if(NOT tree STREQUAL ".")
        set(_install_dest "${_install_dest}/${tree}")
    endif()
    install(DIRECTORY "${src}/"
            DESTINATION "${_install_dest}"
            ${_component_arg}
            USE_SOURCE_PERMISSIONS
            PATTERN ".git" EXCLUDE)
endfunction()

function(_meios_deploy_tree name dir tree dest install_dest component out_command out_label)
    # Each entry keeps its own directory name at the destination, because package://<name>/…
    # resolves to <package root>/<name>/…: copying a selected package's *contents* into the
    # package root would strip the very name the reference is looked up under.
    if(tree STREQUAL ".")
        set(_src "${dir}")
        set(_dst "${dest}")
        set(_label "${name}")
    else()
        set(_src "${dir}/${tree}")
        set(_dst "${dest}/${tree}")
        set(_label "${name}.${tree}")
    endif()
    if(install_dest)
        _meios_deploy_install("${install_dest}" "${component}" "${tree}" "${_src}")
    endif()
    set(${out_command} COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${_src}" "${_dst}"
        PARENT_SCOPE)
    set(${out_label} "${_label}" PARENT_SCOPE)
endfunction()

function(_meios_deploy_collect target resources packages dest install_dest component out_commands
                               out_labels)
    set(_commands "")
    set(_labels "")
    foreach(_name IN LISTS resources)
        meios_resource_dir("${_name}" _dir)
        _meios_deploy_selection("${target}" "${_name}" "${_dir}" "${resources}" "${packages}"
                                _selection)
        foreach(_tree IN LISTS _selection)
            _meios_deploy_tree("${_name}" "${_dir}" "${_tree}" "${dest}" "${install_dest}"
                               "${component}" _command _label)
            list(APPEND _commands ${_command})
            list(APPEND _labels "${_label}")
        endforeach()
    endforeach()
    set(${out_commands} "${_commands}" PARENT_SCOPE)
    set(${out_labels} "${_labels}" PARENT_SCOPE)
endfunction()

# Place declared resources where a built target expects them. SUBDIR is relative to the target's
# runtime directory; several resources may share one SUBDIR, which is how sibling description
# packages end up under a single package-root directory.
function(meios_target_deploy_resources target)
    set(options       INSTALL_RUNTIME_RELATIVE)
    set(one_value     SUBDIR INSTALL_DESTINATION INSTALL_COMPONENT)
    set(multi_value   RESOURCES PACKAGES)
    cmake_parse_arguments(ARG "${options}" "${one_value}" "${multi_value}" ${ARGN})
    _meios_deploy_validate("${target}" "${ARG_UNPARSED_ARGUMENTS}" "${ARG_RESOURCES}"
                           "${ARG_INSTALL_RUNTIME_RELATIVE}" "${ARG_INSTALL_DESTINATION}")
    _meios_deploy_destination("${target}" "${ARG_SUBDIR}" "${ARG_INSTALL_RUNTIME_RELATIVE}"
                              "${ARG_INSTALL_COMPONENT}" "${ARG_INSTALL_DESTINATION}"
                              ARG_INSTALL_DESTINATION _dest)
    _meios_check_contained_paths("meios_target_deploy_resources(${target})" SUBDIR "${ARG_SUBDIR}")
    _meios_check_contained_paths("meios_target_deploy_resources(${target})" PACKAGES "${ARG_PACKAGES}")
    list(REMOVE_DUPLICATES ARG_RESOURCES)
    list(REMOVE_DUPLICATES ARG_PACKAGES)
    _meios_deploy_collect("${target}" "${ARG_RESOURCES}" "${ARG_PACKAGES}" "${_dest}"
                          "${ARG_INSTALL_DESTINATION}" "${ARG_INSTALL_COMPONENT}" _commands _labels)
    _meios_deploy_next_rule("${target}" _rule)
    _meios_deploy_command("${target}" "${_rule}" "${ARG_RESOURCES}" "${_labels}" "${_dest}"
                          "${_commands}")
endfunction()
