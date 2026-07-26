cmake_minimum_required(VERSION 3.28)
project(meios VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/buildflags.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/MeiosDeclareResource.cmake)

option(MEIOS_BUILD_TESTS "Build unit and compile tests" OFF)
option(MEIOS_BUILD_EXAMPLES "Build examples" OFF)
option(MEIOS_BUILD_DOCS "Build the documentation compile-gate" OFF)
option(MEIOS_BUILD_PROPERTY_TESTS "Build property tests (RapidCheck)" OFF)
option(MEIOS_BUILD_FUZZERS "Build libFuzzer harnesses (Clang only)" OFF)
option(MEIOS_CMAKE_FETCH_DEPS "Use FetchContent for dependencies" OFF)
option(MEIOS_COVERAGE "Instrument first-party targets for coverage (gcc/clang)" OFF)
option(MEIOS_BUILD_SCAN_OBJ "Build meios::scan-obj (dependency-free)" OFF)
option(MEIOS_BUILD_SCAN_COLLADA "Build meios::scan-collada (pugixml)" OFF)
option(MEIOS_BUILD_SCAN_STL "Build meios::scan-stl (dependency-free)" OFF)
option(MEIOS_BUILD_SCAN_GLTF "Build meios::scan-gltf (nlohmann/json)" OFF)
option(MEIOS_BUILD_ROS "Build meios::ros (pugixml)" OFF)
option(MEIOS_BUILD_ARCHIVE_ZIP "Build meios::archive-zip (miniz)" OFF)
option(MEIOS_BUILD_EVAL_PYTHON "Build meios::eval-python (pybind11 + found Python3)" OFF)
option(MEIOS_BUILD_TOOLS "Build the meios CLI" OFF)

# Install is gated off only for a backend whose link edge cannot be re-resolved by
# the config package (pybind11-embed); extend the condition as such backends land.
if(NOT MEIOS_BUILD_EVAL_PYTHON)
    set(MEIOS_ENABLE_INSTALL ON)
else()
    set(MEIOS_ENABLE_INSTALL OFF)
endif()

# Config-package gate flags; lib/ flips each to TRUE via PARENT_SCOPE when the
# module that carries the dependency is built.
set(MEIOS_CORE_PUGIXML_INSTALLED FALSE)
set(MEIOS_SCAN_GLTF_INSTALLED FALSE)
set(MEIOS_ARCHIVE_ZIP_INSTALLED FALSE)

# Set before the subdirectories so lib/ and tests/ share one acquisition policy.
# NEVER forces every dependency down the fetch path; ALWAYS prefers a discoverable
# find_package and fetches only the pinned source as a fallback.
if(MEIOS_CMAKE_FETCH_DEPS)
    set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE NEVER)
else()
    set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE ALWAYS)
endif()

add_subdirectory(lib)

# Emitted after lib/ so the export sees every module that joined meiosTargets, and
# the config file is substituted last so the PARENT_SCOPE gate flags are final.
if(MEIOS_ENABLE_INSTALL)
    set(MEIOS_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/meios)
    install(EXPORT meiosTargets
        FILE meiosTargets.cmake
        NAMESPACE meios::
        DESTINATION ${MEIOS_INSTALL_CMAKEDIR})
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/meiosConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/meiosConfig.cmake
        INSTALL_DESTINATION ${MEIOS_INSTALL_CMAKEDIR})
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/meiosConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMinorVersion)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/meiosConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/meiosConfigVersion.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MeiosDeclareResource.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MeiosPruneResources.cmake
        DESTINATION ${MEIOS_INSTALL_CMAKEDIR})
endif()

# Added before the tests block so the meios_cli support lib is a live TARGET when
# the CLI test stems configure. The EXISTS guard keeps a pre-tools tree valid.
if(MEIOS_BUILD_TOOLS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/CMakeLists.txt)
    add_subdirectory(tools)
endif()

# Reaches lib/'s build-tree meios::* targets, so it must follow add_subdirectory(lib);
# the gate compiles the documented snippets against those targets, never the install.
if(MEIOS_BUILD_DOCS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/docs/CMakeLists.txt)
    add_subdirectory(docs)
endif()

if(MEIOS_BUILD_TESTS OR MEIOS_BUILD_PROPERTY_TESTS OR MEIOS_BUILD_EXAMPLES)
    enable_testing()
endif()
if(MEIOS_BUILD_TESTS OR MEIOS_BUILD_PROPERTY_TESTS)
    add_subdirectory(tests)
endif()

# Reaches lib/'s build-tree meios::* targets and registers CTest cases, so it must
# follow both add_subdirectory(lib) and enable_testing(). The EXISTS guard keeps a
# pre-examples tree valid.
if(MEIOS_BUILD_EXAMPLES AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt)
    add_subdirectory(examples)
endif()

# libFuzzer's -fsanitize=fuzzer is a Clang toolchain feature; the harnesses are
# skipped whole on any other compiler rather than failing to link.
if(MEIOS_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_subdirectory(tests/fuzz)
endif()
