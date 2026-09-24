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

set(_args RESOURCES "${FX_NAME}")
if(FX_PACKAGES)
    string(REPLACE "," ";" _packages "${FX_PACKAGES}")
    list(APPEND _args PACKAGES ${_packages})
endif()
set(_subdir "")
if(FX_SUBDIR)
    set(_subdir SUBDIR "${FX_SUBDIR}")
endif()

meios_target_deploy_resources(app ${_args} ${_subdir})
if(FX_SECOND_SUBDIR)
    meios_target_deploy_resources(app ${_args} SUBDIR "${FX_SECOND_SUBDIR}")
endif()

# Read from the published list rather than rebuilt here, because that list is the contract a
# consumer orders against.
get_property(_rules GLOBAL PROPERTY MEIOS_DEPLOY_TARGETS_app_${FX_NAME})
set(_unique "${_rules}")
list(REMOVE_DUPLICATES _unique)
if(NOT _rules OR NOT _unique STREQUAL _rules)
    message(FATAL_ERROR "MEIOS_HARNESS_ASSERT_FAILED: deploy rules empty or repeated: '${_rules}'")
endif()
string(LENGTH "app_deploy_01234567" _bound)
foreach(_rule IN LISTS _rules)
    string(LENGTH "${_rule}" _length)
    if(NOT TARGET "${_rule}" OR _length GREATER _bound)
        message(FATAL_ERROR "MEIOS_HARNESS_ASSERT_FAILED: '${_rule}' is not a bounded target.")
    endif()
endforeach()
