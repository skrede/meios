cmake_minimum_required(VERSION 3.28)

# Three executables built into one runtime directory, the place CMake builds every executable of a
# directory by default, each placing the same tree there.
project(meios_cmake_shared_dir LANGUAGES CXX)

# A consumer requiring CMake 3.31 or later gets CMP0175 as NEW, under which an append carrying a
# keyword it ignores is an error.
if(POLICY CMP0175)
    cmake_policy(SET CMP0175 NEW)
endif()

# Every per-configuration variant is pinned to the same directory as the base one, because a
# multi-config generator otherwise appends the configuration name and the shared directory would
# move with whichever configuration the driver happened to build. The directory name carries a space
# so that every argument the steps pass is quoted on its way through each generator's shell.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY                "${CMAKE_BINARY_DIR}/run dir")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}/run dir")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}/run dir")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/run dir")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_BINARY_DIR}/run dir")

add_executable(app_a main.cpp)
add_executable(app_b main.cpp)
add_executable(app_c main.cpp)

include(MeiosDeclareResource)
meios_declare_resource(NAME demo URL "${FX_URL}" HASH "${FX_HASH}")

if(NOT FX_FLATTEN_ONLY)
    meios_target_deploy_resources(app_a RESOURCES demo SUBDIR models)
    meios_target_deploy_resources(app_b RESOURCES demo SUBDIR models)
    # Its destination lies inside the other two's, which a lock keyed on the destination alone would
    # not serialize.
    meios_target_deploy_resources(app_c RESOURCES demo PACKAGES pkg_b SUBDIR models)
endif()

# Three flattens of one description to one OUTPUT publish identical bytes, so whichever runs last
# leaves the same document.
if(FX_OUTPUT)
    foreach(_app IN ITEMS app_a app_b app_c)
        meios_target_flatten_resource(${_app}
            RESOURCE demo INPUT "${FX_INPUT}" OUTPUT "${FX_OUTPUT}")
    endforeach()
endif()

# The driver takes, from outside the build, the lock a step takes, so it needs the two values the
# step derives it from, spelled as the step receives them. The value is the same in every
# configuration because every configuration is pinned to one directory.
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/harness_lock.txt"
     CONTENT "${CMAKE_BINARY_DIR}\n$<TARGET_FILE_DIR:app_a>\n")

include("${CMAKE_CURRENT_LIST_DIR}/../meios_fixture_checks.cmake")
meios_fixture_require_one_step(app_a "app_a;app_b;app_c")
meios_fixture_require_one_step(app_b "app_a;app_b;app_c")
meios_fixture_require_one_step(app_c "app_a;app_b;app_c")
