cmake_minimum_required(VERSION 3.28)

# CXX rather than NONE: deployment resolves its destination through the target's runtime directory
# and attaches itself with add_dependencies, and neither accepts an imported or a custom target, so
# only a real binary target reaches this path at all.
project(meios_cmake_target LANGUAGES CXX)

# Every per-configuration variant is pinned to the same directory as the base one, because a
# multi-config generator otherwise appends the configuration name and the deployment root would move
# with whichever configuration the driver happened to build.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY                "${CMAKE_BINARY_DIR}/run")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}/run")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}/run")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/run")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_BINARY_DIR}/run")

add_executable(app main.cpp)

include(MeiosDeclareResource)
if(NOT FX_NAME)
    set(FX_NAME demo)
endif()
meios_declare_resource(NAME "${FX_NAME}" URL "${FX_URL}" HASH "${FX_HASH}" OUT_DIR _tree)

# The acquired tree sits at a digest-named path only the module can derive, and the driver has to
# edit a file inside it to prove an edit alone redeploys.
file(WRITE "${CMAKE_BINARY_DIR}/harness_tree.txt" "${_tree}")

set(_names "${FX_NAME}")
if(FX_SECOND_RESOURCE)
    # pkg_b rather than the whole archive, so this tree lands where the first one's does not.
    meios_declare_resource(NAME "${FX_SECOND_RESOURCE}" URL "${FX_URL}" HASH "${FX_HASH}" SUBDIR pkg_b)
    list(APPEND _names "${FX_SECOND_RESOURCE}")
endif()

set(_args RESOURCES ${_names})
if(FX_PACKAGES)
    string(REPLACE "," ";" _packages "${FX_PACKAGES}")
    list(APPEND _args PACKAGES ${_packages})
endif()
set(_subdir "")
if(FX_SUBDIR)
    set(_subdir SUBDIR "${FX_SUBDIR}")
endif()

set(_calls 1)
meios_target_deploy_resources(app ${_args} ${_subdir})
if(FX_SECOND_SUBDIR)
    set(_calls 2)
    meios_target_deploy_resources(app ${_args} SUBDIR "${FX_SECOND_SUBDIR}")
endif()

# Read from the published list rather than rebuilt here, because that list is the contract a
# consumer orders against.
set(_union "")
foreach(_name IN LISTS _names)
    get_property(_rules GLOBAL PROPERTY MEIOS_DEPLOY_TARGETS_app_${_name})
    list(LENGTH _rules _count)
    if(NOT _count EQUAL _calls)
        message(FATAL_ERROR "MEIOS_HARNESS_ASSERT_FAILED: '${_name}' published ${_count} deploy "
                            "rules for ${_calls} calls: '${_rules}'")
    endif()
    list(APPEND _union ${_rules})
endforeach()
list(REMOVE_DUPLICATES _union)
list(LENGTH _union _count)
if(NOT _count EQUAL _calls)
    message(FATAL_ERROR "MEIOS_HARNESS_ASSERT_FAILED: the resources published ${_count} distinct "
                        "deploy rules for ${_calls} calls: '${_union}'")
endif()
foreach(_rule IN LISTS _union)
    if(NOT TARGET "${_rule}" OR NOT _rule MATCHES "^app_deploy(_[0-9]+)?$")
        message(FATAL_ERROR "MEIOS_HARNESS_ASSERT_FAILED: '${_rule}' is not a readable deploy target.")
    endif()
endforeach()
