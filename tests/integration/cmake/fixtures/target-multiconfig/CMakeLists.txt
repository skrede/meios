cmake_minimum_required(VERSION 3.28)

project(meios_cmake_target_multiconfig LANGUAGES CXX)

# A consumer requiring CMake 3.31 or later gets CMP0175 as NEW, under which an append carrying a
# keyword it ignores is an error.
if(POLICY CMP0175)
    cmake_policy(SET CMP0175 NEW)
endif()

# The per-configuration variants are deliberately left unset, which is what makes a multi-config
# generator append the configuration name and give each configuration its own runtime directory.
# Its sibling fixture pins them all to one directory and so cannot observe that at all.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/run")

add_executable(app main.cpp)

include(MeiosDeclareResource)
meios_declare_resource(NAME demo URL "${FX_URL}" HASH "${FX_HASH}" OUT_DIR _tree)

meios_target_deploy_resources(app RESOURCES demo SUBDIR models)

include("${CMAKE_CURRENT_LIST_DIR}/../meios_fixture_checks.cmake")
meios_fixture_require_one_step(app app)
