include(GNUInstallDirs)
include(FetchContent)

# One helper expresses the whole module pattern. A module's public include root is
# lib/meios-<name>/include, so every target is consumed via #include <meios/...>;
# compiled modules also see lib/meios-<name>/src privately. Every module joins the
# single meiosTargets export.
function(meios_add_module name)
    cmake_parse_arguments(ARG "" "TYPE;EXPORT_NAME"
        "SOURCES;LINK_PUBLIC;LINK_PRIVATE;ALIASES" ${ARGN})

    set(target meios_${name})
    set(module_dir ${CMAKE_CURRENT_SOURCE_DIR}/meios-${name})
    set(include_root ${module_dir}/include)

    if(ARG_TYPE STREQUAL "INTERFACE")
        add_library(${target} INTERFACE)
        target_compile_features(${target} INTERFACE cxx_std_20)
        target_include_directories(${target} INTERFACE
            $<BUILD_INTERFACE:${include_root}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
        if(ARG_LINK_PUBLIC)
            target_link_libraries(${target} INTERFACE ${ARG_LINK_PUBLIC})
        endif()
    else()
        add_library(${target} ${ARG_TYPE} ${ARG_SOURCES})
        meios_enable_coverage(${target})
        meios_warnings(${target})
        target_compile_features(${target} PUBLIC cxx_std_20)
        # A static meios archive is meant to embed inside a consumer's shared
        # library; without PIC that final link fails to relocate on Linux.
        set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
        target_include_directories(${target}
            PUBLIC
                $<BUILD_INTERFACE:${include_root}>
                $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
            PRIVATE
                ${module_dir}/src)
        if(ARG_LINK_PUBLIC)
            target_link_libraries(${target} PUBLIC ${ARG_LINK_PUBLIC})
        endif()
        if(ARG_LINK_PRIVATE)
            target_link_libraries(${target} PRIVATE ${ARG_LINK_PRIVATE})
        endif()
    endif()

    if(NOT ARG_EXPORT_NAME)
        set(ARG_EXPORT_NAME ${name})
    endif()
    # CMake aliases are not exported; EXPORT_NAME is what keeps the installed
    # spelling (meios::core) identical to the build-tree alias.
    set_target_properties(${target} PROPERTIES EXPORT_NAME ${ARG_EXPORT_NAME})

    foreach(alias ${ARG_ALIASES})
        add_library(${alias} ALIAS ${target})
    endforeach()

    if(MEIOS_ENABLE_INSTALL)
        install(TARGETS ${target}
            EXPORT meiosTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        if(EXISTS ${include_root})
            install(DIRECTORY ${include_root}/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
        endif()
    endif()
endfunction()

# Each dependency is declared once with FIND_PACKAGE_ARGS, so a single declaration
# expresses both acquisition paths (the top-level FETCHCONTENT_TRY_FIND_PACKAGE_MODE
# picks which one). A found pugixml keeps meios::core's exported target an imported
# reference; a fetched (non-imported) pugixml cannot join install(EXPORT) on the
# $<LINK_ONLY:> edge, which is why the fetched case disables install below.
set(PUGIXML_BUILD_TESTS OFF)
set(PUGIXML_INSTALL OFF)
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG c8033ce9d039e7f9d134877c363397b3cfe20816  # pugixml v1.16
    SYSTEM
    FIND_PACKAGE_ARGS 1.16 NAMES pugixml
)
FetchContent_MakeAvailable(pugixml)

# A fetched pugixml is a real, non-imported target outside meiosTargets, so the
# exported meios::core cannot resolve its private link edge from an installed tree.
# Installing meios therefore requires a discoverable (find_package-able) pugixml.
if(pugixml_SOURCE_DIR)
    message(STATUS
        "meios: pugixml was fetched; disabling install "
        "(the installed package requires a find_package-able pugixml >= 1.16).")
    set(MEIOS_ENABLE_INSTALL OFF)
    set(MEIOS_ENABLE_INSTALL OFF PARENT_SCOPE)
endif()

# Opt-in enrichment backends: pinned to immutable refs now so every dependency is
# recorded as FetchContent-able, but only made available when their target builds.
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG 65ee68451d8eb2b5f3a30b410476ab83deb3289b  # nlohmann_json v3.12.0
    SYSTEM
    FIND_PACKAGE_ARGS 3.12 NAMES nlohmann_json
)
FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG 77d0dce8627735138c51770d1799a1ef48f2117d  # miniz 3.1.2
    SYSTEM
    FIND_PACKAGE_ARGS NAMES miniz
)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG c7fb32eea8c92bebeea9f0735041a72aa20c75f5  # pybind11 v3.0.4
    SYSTEM
    FIND_PACKAGE_ARGS NAMES pybind11
)
if(MEIOS_BUILD_SCAN_GLTF)
    set(JSON_BuildTests OFF)
    FetchContent_MakeAvailable(nlohmann_json)
endif()
if(MEIOS_BUILD_ARCHIVE_ZIP)
    FetchContent_MakeAvailable(miniz)
endif()
if(MEIOS_BUILD_EVAL_PYTHON)
    FetchContent_MakeAvailable(pybind11)
endif()

# core: the compiled engine. pugixml is linked PRIVATE and is reachable only from
# lib/meios-core/src, so it never appears in meios::core's public requirements.
# Every module's compiled body lands under meios-core/src, so the source list is a
# recursive glob: a new .cpp joins the archive with no CMake edit.
file(GLOB_RECURSE meios_core_sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/meios-core/src/*.cpp)
meios_add_module(core
    TYPE STATIC
    EXPORT_NAME core
    ALIASES meios::core
    SOURCES ${meios_core_sources}
    LINK_PRIVATE pugixml::pugixml)
set_target_properties(meios_core PROPERTIES VERSION ${PROJECT_VERSION})

configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/meios-core/include/meios/version.h
    @ONLY)
target_include_directories(meios_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/meios-core/include>)
if(MEIOS_ENABLE_INSTALL)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/meios-core/include/meios/version.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/meios)
endif()

# As a static archive, meios::core's PRIVATE pugixml survives in the exported
# interface as $<LINK_ONLY:pugixml::pugixml>, so the installed package config must
# resolve pugixml. Record that it must; the config template gates on this.
set(MEIOS_CORE_PUGIXML_INSTALLED TRUE PARENT_SCOPE)

meios_add_module(model TYPE INTERFACE ALIASES meios::model)
meios_add_module(completion TYPE INTERFACE ALIASES meios::completion)

# The header modules re-export the diagnostic seam, which lives only in the
# header-only meios::model; PUBLIC-linking it is a header-visibility edge that adds
# no compiled-archive dependency, so the single pugixml PRIVATE edge is preserved.
meios_add_module(xacro TYPE INTERFACE ALIASES meios::xacro LINK_PUBLIC meios::core meios::model)
meios_add_module(io TYPE INTERFACE ALIASES meios::io LINK_PUBLIC meios::core meios::model)

# The INTERFACE modules PUBLIC-link core, so their include roots do not flow back
# to the core archive; compiled bodies under meios-core/src reach the module and
# diagnostic headers only through these PRIVATE roots (header visibility, no link).
target_include_directories(meios_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/meios-xacro/include
    ${CMAKE_CURRENT_SOURCE_DIR}/meios-io/include
    ${CMAKE_CURRENT_SOURCE_DIR}/meios-model/include)
