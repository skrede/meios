cmake_minimum_required(VERSION 3.28)

# CXX rather than NONE: the flatten rule writes into the target's runtime directory and attaches
# itself with add_dependencies, so only a real binary target reaches this path at all.
project(meios_cmake_flatten LANGUAGES CXX)

# Every per-configuration variant is pinned to the same directory as the base one, because a
# multi-config generator otherwise appends the configuration name and the flattened document would
# move with whichever configuration the driver happened to build.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY                "${CMAKE_BINARY_DIR}/run")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}/run")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}/run")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/run")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_BINARY_DIR}/run")

add_executable(app main.cpp)

include(MeiosDeclareResource)
meios_declare_resource(NAME demo URL "${FX_URL}" HASH "${FX_HASH}" OUT_DIR _tree)

# The acquired tree sits at a digest-named path only the module can derive, and the driver has to
# edit the description inside it to prove an edit alone re-expands.
file(WRITE "${CMAKE_BINARY_DIR}/harness_tree.txt" "${_tree}")

# Deployment is present so the flatten call has a deploy target to order against, which is the
# arrangement a consumer actually writes: the tree lands beside the document that was expanded
# out of it.
set(_deploy RESOURCES demo SUBDIR models)
if(FX_DEPLOY_INSTALL_RUNTIME_RELATIVE)
    list(APPEND _deploy INSTALL_RUNTIME_RELATIVE)
endif()
meios_target_deploy_resources(app ${_deploy})

set(_call RESOURCE demo INPUT "${FX_INPUT}" OUTPUT "${FX_OUTPUT}")
if(FX_ARGS)
    string(REPLACE "," ";" _overrides "${FX_ARGS}")
    list(APPEND _call ARGS ${_overrides})
endif()
if(FX_PACKAGE_PATH)
    string(REPLACE "," ";" _roots "${FX_PACKAGE_PATH}")
    list(APPEND _call PACKAGE_PATH ${_roots})
endif()
if(FX_EVAL)
    list(APPEND _call EVAL "${FX_EVAL}")
endif()
if(FX_INSTALL_DESTINATION)
    list(APPEND _call INSTALL_DESTINATION "${FX_INSTALL_DESTINATION}")
endif()
if(FX_INSTALL_COMPONENT)
    list(APPEND _call INSTALL_COMPONENT "${FX_INSTALL_COMPONENT}")
endif()
if(FX_INSTALL_RUNTIME_RELATIVE)
    list(APPEND _call INSTALL_RUNTIME_RELATIVE)
endif()

meios_target_flatten_resource(app ${_call})
