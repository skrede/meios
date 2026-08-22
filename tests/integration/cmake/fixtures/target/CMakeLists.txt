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
meios_declare_resource(NAME demo URL "${FX_URL}" HASH "${FX_HASH}" OUT_DIR _tree)

# The acquired tree sits at a digest-named path only the module can derive, and the driver has to
# edit a file inside it to prove an edit alone redeploys.
file(WRITE "${CMAKE_BINARY_DIR}/harness_tree.txt" "${_tree}")

set(_args RESOURCES demo)
if(FX_SUBDIR)
    list(APPEND _args SUBDIR "${FX_SUBDIR}")
endif()
if(FX_PACKAGES)
    string(REPLACE "," ";" _packages "${FX_PACKAGES}")
    list(APPEND _args PACKAGES ${_packages})
endif()

meios_target_deploy_resources(app ${_args})
