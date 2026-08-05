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

    if(MEIOS_INSTALL)
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

# One recorded fact per acquired dependency, in a global property rather than a scoped variable
# because the decision that reads them back is taken in a directory that ran none of these sites.
function(_meios_record_dependency_install name option source_dir)
    set(_fetched FALSE)
    set(_installs FALSE)
    if(source_dir)
        set(_fetched TRUE)
    endif()
    if(${option})
        set(_installs TRUE)
    endif()
    set_property(GLOBAL APPEND PROPERTY MEIOS_DEPENDENCY_INSTALL_FACTS
        "${name}|${option}|${_fetched}|${_installs}")
endfunction()

# Each dependency is declared once with FIND_PACKAGE_ARGS, so a single declaration
# expresses both acquisition paths (the top-level FETCHCONTENT_TRY_FIND_PACKAGE_MODE
# picks which one). A found pugixml keeps meios::core's exported target an imported
# reference; a fetched (non-imported) pugixml cannot join install(EXPORT) on the
# $<LINK_ONLY:> edge, which is why a fetched dependency that declines to install
# itself disables meios's own install below.
# GLOBAL: pugixml is the one dependency linked from outside this directory.
set(PUGIXML_BUILD_TESTS OFF)
# pugixml installs itself by default and meios declines on the consumer's behalf, so a fetched
# dependency is not deployed into a prefix nobody asked to receive it. A consumer that does want
# it says so before adding meios, which is also what makes meios's own export legal when pugixml
# was fetched rather than found.
if(NOT DEFINED PUGIXML_INSTALL)
    set(PUGIXML_INSTALL OFF)
endif()
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG c8033ce9d039e7f9d134877c363397b3cfe20816  # pugixml v1.16
    SYSTEM
    FIND_PACKAGE_ARGS 1.16 NAMES pugixml GLOBAL
)
FetchContent_MakeAvailable(pugixml)
_meios_record_dependency_install(pugixml PUGIXML_INSTALL "${pugixml_SOURCE_DIR}")

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
    _meios_record_dependency_install(nlohmann_json JSON_Install "${nlohmann_json_SOURCE_DIR}")
endif()
if(MEIOS_BUILD_ARCHIVE_ZIP)
    FetchContent_MakeAvailable(miniz)
    _meios_record_dependency_install(miniz INSTALL_PROJECT "${miniz_SOURCE_DIR}")
endif()
if(MEIOS_BUILD_EVAL_PYTHON)
    FetchContent_MakeAvailable(pybind11)
    _meios_record_dependency_install(pybind11 PYBIND11_INSTALL "${pybind11_SOURCE_DIR}")
endif()

# Read once here, the last point at which the decision can still reach the module helper. A fetched
# dependency that does not install itself is a real, non-imported target belonging to no export
# set, so meios's own export would fail to generate around it.
set(_meios_declining "")
if(MEIOS_INSTALL)
    get_property(_meios_facts GLOBAL PROPERTY MEIOS_DEPENDENCY_INSTALL_FACTS)
    foreach(_fact IN LISTS _meios_facts)
        string(REPLACE "|" ";" _field "${_fact}")
        list(GET _field 0 _name)
        list(GET _field 1 _option)
        list(GET _field 2 _fetched)
        list(GET _field 3 _installs)
        if(_fetched AND NOT _installs)
            list(APPEND _meios_declining "${_name} (turn ${_option} on)")
        endif()
    endforeach()
endif()
if(_meios_declining)
    string(REPLACE ";" ", " _meios_declining "${_meios_declining}")
    message(STATUS
        "meios: not installing, because a fetched dependency declines to install itself and so "
        "belongs to no export set: ${_meios_declining}. Nothing meios builds reaches your prefix, "
        "including meios's CMake resource modules. Supply a discoverable version of it instead, "
        "or turn the named option on before adding meios.")
    # Under a consumer this reaches meios's own directory scope and stops there; a variable of the
    # same name in the consumer's scope is untouched, which is what it should be.
    set(MEIOS_INSTALL OFF)
    set(MEIOS_INSTALL OFF PARENT_SCOPE)
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
if(MEIOS_INSTALL)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/meios-core/include/meios/version.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/meios)
endif()

# As a static archive, meios::core's PRIVATE pugixml survives in the exported
# interface as $<LINK_ONLY:pugixml::pugixml>, so the installed package config must
# resolve pugixml. Record that it must; the config template gates on this.
set(MEIOS_CORE_PUGIXML_INSTALLED TRUE PARENT_SCOPE)

meios_add_module(model TYPE INTERFACE ALIASES meios::model)
meios_add_module(completion TYPE INTERFACE ALIASES meios::completion
    LINK_PUBLIC meios::core meios::model)

# The header modules re-export the diagnostic seam, which lives only in the
# header-only meios::model; PUBLIC-linking it is a header-visibility edge that adds
# no compiled-archive dependency, so the single pugixml PRIVATE edge is preserved.
meios_add_module(xacro TYPE INTERFACE ALIASES meios::xacro LINK_PUBLIC meios::core meios::model)
meios_add_module(io TYPE INTERFACE ALIASES meios::io LINK_PUBLIC meios::core meios::model)
meios_add_module(urdf TYPE INTERFACE ALIASES meios::urdf
    LINK_PUBLIC meios::core meios::model meios::io meios::xacro)
meios_add_module(bundle TYPE INTERFACE ALIASES meios::bundle
    LINK_PUBLIC meios::core meios::model meios::io)

# The INTERFACE modules PUBLIC-link core, so their include roots do not flow back
# to the core archive; compiled bodies under meios-core/src reach the module and
# diagnostic headers only through these PRIVATE roots (header visibility, no link).
target_include_directories(meios_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/meios-xacro/include
    ${CMAKE_CURRENT_SOURCE_DIR}/meios-io/include
    ${CMAKE_CURRENT_SOURCE_DIR}/meios-model/include
    ${CMAKE_CURRENT_SOURCE_DIR}/meios-urdf/include
    ${CMAKE_CURRENT_SOURCE_DIR}/meios-bundle/include
    ${CMAKE_CURRENT_SOURCE_DIR}/meios-completion/include)

# Opt-in enrichment modules. Each is its own compiled archive whose body lives
# under lib/meios-<name>/src, never meios-core/src, so the always-on core glob
# never absorbs an enrichment .cpp. A module target only materializes once its
# src carries a source; the glob guard keeps an options-on, sources-absent tree
# valid so an implementing plan adds only .cpp files.
function(meios_add_enrichment name)
    file(GLOB_RECURSE enrichment_sources CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/meios-${name}/src/*.cpp)
    if(NOT enrichment_sources)
        return()
    endif()
    meios_add_module(${name} TYPE STATIC ALIASES meios::${name}
        SOURCES ${enrichment_sources} ${ARGN})
endfunction()

if(MEIOS_BUILD_SCAN_OBJ)
    meios_add_enrichment(scan-obj LINK_PUBLIC meios::bundle meios::io)
endif()
if(MEIOS_BUILD_SCAN_STL)
    meios_add_enrichment(scan-stl LINK_PUBLIC meios::bundle meios::io)
endif()
if(MEIOS_BUILD_SCAN_COLLADA)
    meios_add_enrichment(scan-collada
        LINK_PUBLIC meios::bundle meios::io
        LINK_PRIVATE pugixml::pugixml)
endif()
if(MEIOS_BUILD_SCAN_GLTF)
    meios_add_enrichment(scan-gltf
        LINK_PUBLIC meios::bundle meios::io
        LINK_PRIVATE nlohmann_json::nlohmann_json)
    if(TARGET meios_scan-gltf)
        set(MEIOS_SCAN_GLTF_INSTALLED TRUE PARENT_SCOPE)
    endif()
endif()
if(MEIOS_ROS_PACKAGE_SUPPORT)
    meios_add_enrichment(ros
        LINK_PUBLIC meios::io
        LINK_PRIVATE pugixml::pugixml)
endif()
if(MEIOS_BUILD_ARCHIVE_ZIP)
    meios_add_enrichment(archive-zip
        LINK_PUBLIC meios::bundle meios::io
        LINK_PRIVATE miniz)
    if(TARGET meios_archive-zip)
        set(MEIOS_ARCHIVE_ZIP_INSTALLED TRUE PARENT_SCOPE)
    endif()
endif()

# eval-python drives a found (never fetched) interpreter; if Python3's embed
# component is absent the target is skipped rather than failing the configure.
if(MEIOS_BUILD_EVAL_PYTHON)
    if(MEIOS_REQUIRE_EVAL_PYTHON)
        find_package(Python3 QUIET COMPONENTS Interpreter Development.Embed)
        if(NOT Python3_FOUND)
            message(FATAL_ERROR
                "meios: MEIOS_REQUIRE_EVAL_PYTHON is ON but Python3 Development.Embed was not found, "
                "so meios::eval-python would have been skipped.")
        endif()
    else()
        find_package(Python3 COMPONENTS Interpreter Development.Embed)
    endif()
    if(Python3_FOUND)
        meios_add_enrichment(eval-python
            LINK_PUBLIC meios::xacro
            LINK_PRIVATE pybind11::embed)
    else()
        message(STATUS
            "meios: Python3 Development.Embed not found; skipping meios::eval-python.")
    endif()
    # The enrichment helper also returns silently when the module's source glob comes
    # back empty, which a found interpreter says nothing about; a required build has to
    # check the target rather than the discovery result.
    if(MEIOS_REQUIRE_EVAL_PYTHON AND NOT TARGET meios_eval-python)
        message(FATAL_ERROR
            "meios: MEIOS_REQUIRE_EVAL_PYTHON is ON but meios::eval-python did not materialize.")
    endif()
endif()
