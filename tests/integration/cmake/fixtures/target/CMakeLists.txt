cmake_minimum_required(VERSION 3.28)

# CXX rather than NONE: deployment resolves its destination through the target's runtime directory,
# which neither an imported nor a custom target has, so only a real binary target reaches this path.
project(meios_cmake_target LANGUAGES CXX)

# A consumer requiring CMake 3.31 or later gets CMP0175 as NEW, under which an append carrying a
# keyword it ignores is an error.
if(POLICY CMP0175)
    cmake_policy(SET CMP0175 NEW)
endif()

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

if(FX_ELSEWHERE)
    add_subdirectory(elsewhere)
endif()

set(_args RESOURCES ${_names})
if(DEFINED FX_PACKAGES)
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

include("${CMAKE_CURRENT_LIST_DIR}/../meios_fixture_checks.cmake")
meios_fixture_require_one_step(app app)
