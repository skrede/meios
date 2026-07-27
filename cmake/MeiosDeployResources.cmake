include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/MeiosResourcePaths.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/MeiosResourceRegistry.cmake")

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
                                   out_install out_dest out_slot)
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
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _slot "${subdir}")
    set(${out_install} "${install_dest}" PARENT_SCOPE)
    set(${out_dest} "${_dest}" PARENT_SCOPE)
    set(${out_slot} "${_slot}" PARENT_SCOPE)
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

# Driven by an OUTPUT rule keyed on the tree's contents rather than a POST_BUILD
# command: POST_BUILD runs only when the target itself relinks, so editing a resource
# without touching a source file would leave a stale tree deployed with no sign
# anything was wrong. copy_directory_if_different keeps an unchanged tree from
# restamping every file.
function(_meios_deploy_command target slot label src dst out_rule)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/meios_deploy")
    file(GLOB_RECURSE _files CONFIGURE_DEPENDS "${src}/*")
    # Keyed on $<CONFIG> because the destination is: under a multi-config generator each
    # configuration has its own runtime directory, and one shared stamp would let the first
    # configuration built mark the rest up to date and leave them without the tree. The
    # configuration goes in the file name, not a directory, and the label is slugged for the
    # same reason — `cmake -E touch` does not create parents, and a generator expression
    # cannot be resolved at configure time to create them.
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _label_slot "${label}")
    set(_stamp
        "${CMAKE_CURRENT_BINARY_DIR}/meios_deploy/${target}.${slot}.${_label_slot}.$<CONFIG>.stamp")
    add_custom_command(
        OUTPUT  "${_stamp}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${src}" "${dst}"
        COMMAND ${CMAKE_COMMAND} -E touch "${_stamp}"
        DEPENDS ${_files}
        COMMENT "Deploying resource '${label}' to ${dst}"
        VERBATIM)
    set(_rule ${target}_deploy_${slot}_${_label_slot})
    add_custom_target(${_rule} DEPENDS "${_stamp}")
    add_dependencies(${target} ${_rule})
    set(${out_rule} "${_rule}" PARENT_SCOPE)
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

function(_meios_deploy_rule target name dir tree dest slot install_dest component)
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
    _meios_deploy_command("${target}" "${slot}" "${_label}" "${_src}" "${_dst}" _rule)
    # Published rather than left to be reconstructed: a module ordering against this rule would
    # otherwise couple itself to a target-name format that the next edit here breaks.
    set_property(GLOBAL APPEND PROPERTY MEIOS_DEPLOY_TARGETS_${target}_${name} "${_rule}")
    if(install_dest)
        _meios_deploy_install("${install_dest}" "${component}" "${tree}" "${_src}")
    endif()
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
                              ARG_INSTALL_DESTINATION _dest _slot)
    _meios_check_contained_paths("meios_target_deploy_resources(${target})" SUBDIR "${ARG_SUBDIR}")
    _meios_check_contained_paths("meios_target_deploy_resources(${target})" PACKAGES "${ARG_PACKAGES}")

    foreach(_name IN LISTS ARG_RESOURCES)
        meios_resource_dir("${_name}" _dir)
        _meios_deploy_selection("${target}" "${_name}" "${_dir}" "${ARG_RESOURCES}"
                                "${ARG_PACKAGES}" _selection)
        foreach(_tree IN LISTS _selection)
            _meios_deploy_rule("${target}" "${_name}" "${_dir}" "${_tree}" "${_dest}" "${_slot}"
                               "${ARG_INSTALL_DESTINATION}" "${ARG_INSTALL_COMPONENT}")
        endforeach()
    endforeach()
endfunction()
