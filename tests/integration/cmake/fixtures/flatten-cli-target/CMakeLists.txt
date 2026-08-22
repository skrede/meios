cmake_minimum_required(VERSION 3.28)

project(meios_cmake_flatten_cli_target LANGUAGES CXX)

# Every per-configuration variant is pinned to the same directory as the base one, because a
# multi-config generator otherwise appends the configuration name and the flattened document would
# move with whichever configuration the driver happened to build.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY                "${CMAKE_BINARY_DIR}/run")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}/run")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}/run")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/run")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_BINARY_DIR}/run")

add_executable(app main.cpp)

# The binary the flatten rule runs is a target in this same build, named so the module's own
# selection order reaches it: no MEIOS_CLI_EXECUTABLE is set, so the rule resolves the target rather
# than a caller-supplied path. It is placed away from the runtime directory the deployment writes
# into, so a rule reaching it is reaching the target and not a file that happened to land there.
add_executable(meios stub_cli.cpp)
set_target_properties(meios PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY                "${CMAKE_BINARY_DIR}/cli"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}/cli"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}/cli"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/cli"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_BINARY_DIR}/cli")

include(MeiosDeclareResource)
meios_declare_resource(NAME demo URL "${FX_URL}" HASH "${FX_HASH}" OUT_DIR _tree)

file(WRITE "${CMAKE_BINARY_DIR}/harness_tree.txt" "${_tree}")

meios_target_deploy_resources(app RESOURCES demo SUBDIR models)
meios_target_flatten_resource(app RESOURCE demo INPUT "${FX_INPUT}" OUTPUT "${FX_OUTPUT}")
