include(GNUInstallDirs)

# Acquisition first, and the module pattern after it: a fetched dependency that declines to install
# turns the install off, and the module helper reads that decision as it registers each target.
include(${CMAKE_CURRENT_LIST_DIR}/dependencies.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/modules.cmake)

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

if(MEIOS_SCAN_OBJ_SUPPORT)
    meios_add_enrichment(scan-obj LINK_PUBLIC meios::bundle meios::io)
endif()
if(MEIOS_SCAN_STL_SUPPORT)
    meios_add_enrichment(scan-stl LINK_PUBLIC meios::bundle meios::io)
endif()
if(MEIOS_SCAN_COLLADA_SUPPORT)
    meios_add_enrichment(scan-collada
        LINK_PUBLIC meios::bundle meios::io
        LINK_PRIVATE pugixml::pugixml)
endif()
if(MEIOS_SCAN_GLTF_SUPPORT)
    meios_add_enrichment(scan-gltf
        LINK_PUBLIC meios::bundle meios::io
        LINK_PRIVATE nlohmann_json::nlohmann_json)
    if(TARGET meios_scan-gltf)
        set(MEIOS_SCAN_GLTF_INSTALLED TRUE PARENT_SCOPE)
    endif()
endif()
if(MEIOS_ROS_PACKAGE_SUPPORT)
    meios_add_enrichment(ros-package
        LINK_PUBLIC meios::io
        LINK_PRIVATE pugixml::pugixml)
endif()
if(MEIOS_ARCHIVE_ZIP_SUPPORT)
    meios_add_enrichment(archive-zip
        LINK_PUBLIC meios::bundle meios::io
        LINK_PRIVATE miniz::miniz)
    if(TARGET meios_archive-zip)
        set(MEIOS_ARCHIVE_ZIP_INSTALLED TRUE PARENT_SCOPE)
    endif()
endif()

# eval-python drives a found (never fetched) interpreter; if Python3's embed
# component is absent the target is skipped rather than failing the configure.
if(MEIOS_EVAL_PYTHON_SUPPORT)
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
            LINK_PRIVATE pybind11::embed
            NOT_EXPORTABLE
                "its embedded-interpreter link edge cannot be re-resolved from an installed tree")
    else()
        message(STATUS
            "meios: Python3 Development.Embed not found; skipping meios::eval-python.")
    endif()
    # The enrichment helper also returns silently when the module's source glob comes
    # back empty, which a found interpreter says nothing about; a required build has to
    # check the target rather than the discovery result. This asks whether the target
    # exists, never whether it installs: the module is built and linkable in-tree
    # whether or not it joins the installed export, and those are two different absences.
    if(MEIOS_REQUIRE_EVAL_PYTHON AND NOT TARGET meios_eval-python)
        message(FATAL_ERROR
            "meios: MEIOS_REQUIRE_EVAL_PYTHON is ON but meios::eval-python did not materialize.")
    endif()
endif()
